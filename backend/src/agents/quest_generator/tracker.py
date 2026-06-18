"""QuestProgressTracker for handling factory events and updating quest progress."""

from __future__ import annotations

from datetime import UTC, datetime

from sqlalchemy import select
from sqlalchemy.orm import Session

from agents.quest_generator.repository import QuestRepository
from db.models import QuestInstanceModel, QuestProgressModel

RESOURCE_PREFIX = "resource_"


class QuestProgressTracker:
    """공장에서 발생하는 아이템 획득 이벤트(item_collected)를 수신하여 활성 지원 퀘스트의 진행도를 추적 및 갱신합니다.

    현재 MVP 단계에서는 item_collected 이벤트 유형만 처리하도록 구현되어 있습니다.
    """

    @classmethod
    def track_item_collected(
        cls,
        session: Session,
        event_id: str,
        factory_id: str,
        item_id: str,
        current_total: int,
    ) -> None:
        """아이템 획득 이벤트(item_collected)를 처리하여 매칭되는 퀘스트 진행도를 멱등하게 스냅샷 업데이트합니다.

        이 진행도 트래커는 스냅샷 대입 방식으로 진행도를 업데이트합니다. 따라서 보유량이 감소할 경우
        진행도가 감소할 수 있으나, 이미 완료(completed) 처리된 퀘스트는 비가역적으로 완료 상태가 유지됩니다.

        Args:
            session: SQLAlchemy 데이터베이스 세션
            event_id: 중복 처리를 방지하기 위한 이벤트 고유 식별자
            factory_id: 이벤트를 발생시킨 공장 식별자
            item_id: 획득한 아이템 식별자
            current_total: 현재 시점의 아이템 총 보유량 (스냅샷 대입값)
        """
        # 1. 멱등성 검사 (동일 공장에서 해당 event_id가 이미 처리되었는지 전체 이력을 기준으로 중복 방지)
        # 이미 완료된 퀘스트를 포함하여 동일 이벤트가 중복 유입될 때 다른 활성 퀘스트로의 누수를 조기 차단합니다.
        idempotent_stmt = (
            select(QuestProgressModel.id)
            .join(
                QuestInstanceModel,
                QuestProgressModel.quest_instance_id == QuestInstanceModel.id,
            )
            .where(
                QuestInstanceModel.factory_id == factory_id,
                QuestProgressModel.last_event_id == event_id,
            )
        )
        if session.execute(idempotent_stmt).first() is not None:
            return

        # 2. 해당 공장의 모든 활성(in_progress) 퀘스트 진행도 데이터와 생성일자(created_at) 단일 쿼리로 조회
        # TODO: 멀티 컨슈머/워커 환경 도입 시 동시성 경쟁 조건(lost update 등)을 방지하기 위해 SELECT ... FOR UPDATE 적용 필요
        stmt = (
            select(QuestProgressModel, QuestInstanceModel.created_at)
            .join(
                QuestInstanceModel,
                QuestProgressModel.quest_instance_id == QuestInstanceModel.id,
            )
            .where(
                QuestInstanceModel.factory_id == factory_id,
                QuestProgressModel.status == "in_progress",
            )
        )
        result = session.execute(stmt)
        active_rows = result.all()  # 각 요소는 (QuestProgressModel, datetime) 튜플 (result.all()은 이미 list 반환)

        if not active_rows:
            return

        # 3. 아이템 ID 매칭 필터링
        # target_id와 item_id의 접두사 "resource_" 유무에 상관없이 유연하게 매칭
        matched_rows: list[tuple[QuestProgressModel, datetime]] = []
        item_clean = item_id.removeprefix(RESOURCE_PREFIX)
        for prog, created_at in active_rows:
            target_clean = prog.target_id.removeprefix(RESOURCE_PREFIX)
            if target_clean == item_clean:
                matched_rows.append((prog, created_at))

        if not matched_rows:
            return

        # 4. 인스턴스 생성 일자(created_at) 기준 정렬하여 가장 오래된 1개 선택 (Oldest-First)
        # N+1 쿼리 병목을 피하기 위해 1단계에서 단일 쿼리로 가져온 created_at을 이용해 정렬
        matched_rows.sort(key=lambda r: r[1])
        target_progress = matched_rows[0][0]

        # 5. 스냅샷 대입 방식으로 진행도 갱신
        target_progress.current_amount = current_total
        target_progress.last_event_id = event_id

        # 6. 목표량 도달 여부에 따른 완료 처리 전이
        if target_progress.current_amount >= target_progress.target_amount:
            target_progress.status = "completed"

            # 퀘스트 인스턴스 내 모든 목표가 완료되었는지 검사
            instance_id = target_progress.quest_instance_id
            all_progs = QuestRepository.get_progress_by_instance(session, instance_id)
            if all_progs and all(p.status == "completed" for p in all_progs):
                db_instance = QuestRepository.get_instance_by_id(session, instance_id)
                if db_instance and db_instance.status == "in_progress":
                    db_instance.status = "completed"
                    db_instance.completed_at = datetime.now(UTC)
                    QuestRepository.save_instance(session, db_instance)

        QuestRepository.save_progress(session, target_progress)
