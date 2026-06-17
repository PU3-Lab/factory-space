"""QuestManager for managing the lifecycle of support quests."""

from __future__ import annotations

import uuid
from datetime import UTC, datetime

from sqlalchemy.orm import Session

from agents.quest_generator.models import QuestInstance, SupportQuestDraft
from agents.quest_generator.repository import QuestRepository
from agents.quest_generator.reward_resolver import QuestRewardResolver
from db.models import QuestInstanceModel, QuestProgressModel


class QuestManager:
    """지원 퀘스트 인스턴스의 생성, 완료 및 보상 수령 상태 전이를 조율하는 컴포넌트입니다."""

    @classmethod
    def create_quest_from_draft(
        cls,
        session: Session,
        factory_id: str,
        draft: SupportQuestDraft,
        related_main_quest_id: str | None = None,
    ) -> QuestInstance:
        """퀘스트 초안(Draft)을 기반으로 새로운 퀘스트 인스턴스와 하위 목표 진행도 레코드를 생성하여 영속화합니다.

        Args:
            session: SQLAlchemy 데이터베이스 세션
            factory_id: 퀘스트를 할당받을 공장 식별자
            draft: 생성할 지원 퀘스트 초안
            related_main_quest_id: 연관된 메인 퀘스트 식별자 (선택)

        Returns:
            Pydantic으로 변환된 생성 완료 퀘스트 인스턴스 정보
        """
        instance_id = f"qinst_{uuid.uuid4().hex[:8]}"

        # 보상 정제 (순환 파밍 방지 필터링)
        # 단일 목표(collect_item) 형태이므로 첫 번째 objective의 target_id를 기준 타겟으로 삼음
        target_item_id = ""
        if draft.objectives:
            target_item_id = draft.objectives[0].target_id

        resolved_rewards = QuestRewardResolver.resolve_rewards(
            draft.rewards, target_item_id
        )

        # 1. QuestInstanceModel 생성
        db_instance = QuestInstanceModel(
            id=instance_id,
            factory_id=factory_id,
            quest_type=draft.quest_type,
            support_type=draft.support_type,
            related_main_quest_id=related_main_quest_id,
            title=draft.title,
            description=draft.description,
            status="in_progress",
            objective_json=[obj.model_dump() for obj in draft.objectives],
            reward_json=[r.model_dump() for r in resolved_rewards],
            created_by="rule",
            level_reward_applied=False,
            created_at=datetime.now(UTC),
        )

        QuestRepository.save_instance(session, db_instance)

        # 2. 각 objective마다 QuestProgressModel 생성 및 저장
        for obj in draft.objectives:
            progress_id = f"qprog_{uuid.uuid4().hex[:8]}"
            db_progress = QuestProgressModel(
                id=progress_id,
                quest_instance_id=instance_id,
                objective_id=obj.id,
                objective_type=obj.type,
                target_id=obj.target_id,
                current_amount=obj.current_amount,
                target_amount=obj.target_amount,
                status="in_progress",
            )
            QuestRepository.save_progress(session, db_progress)

        return db_instance.to_pydantic()

    @classmethod
    def complete_quest(cls, session: Session, instance_id: str) -> QuestInstance | None:
        """지정된 퀘스트 인스턴스의 상태를 completed(완료)로 전이시킵니다."""
        db_instance = QuestRepository.get_instance_by_id(session, instance_id)
        if not db_instance:
            return None

        # in_progress 상태인 경우에만 완료 전이 허용 (멱등성 보장)
        if db_instance.status != "in_progress":
            return db_instance.to_pydantic()

        db_instance.status = "completed"
        db_instance.completed_at = datetime.now(UTC)
        QuestRepository.save_instance(session, db_instance)

        return db_instance.to_pydantic()

    @classmethod
    def claim_reward(cls, session: Session, instance_id: str) -> QuestInstance | None:
        """지정된 퀘스트 인스턴스의 상태를 reward_claimed(보상 수령 완료) 상태로 전이시킵니다."""
        db_instance = QuestRepository.get_instance_by_id(session, instance_id)
        if not db_instance:
            return None

        # completed 상태인 경우에만 보상 수령 전이 허용 (상태 흐름 보장)
        if db_instance.status != "completed":
            return None

        db_instance.status = "reward_claimed"
        QuestRepository.save_instance(session, db_instance)

        return db_instance.to_pydantic()
