from __future__ import annotations

from fastapi import APIRouter, HTTPException, status

from agents.quest_generator.context_builder import QuestContextBuilder
from agents.quest_generator.manager import QuestManager
from agents.quest_generator.models import (
    ItemCollectedEvent,
    QuestContext,
    QuestInstance,
)
from agents.quest_generator.repository import QuestRepository
from agents.quest_generator.rule_generator import QuestRuleGenerator
from agents.quest_generator.tracker import QuestProgressTracker
from agents.quest_generator.validator import QuestValidator
from db.engine import get_db_session

router = APIRouter()

# 퀘스트 생성 및 처리를 위한 비즈니스 정책 상수
MAX_ACTIVE_SUPPORT_QUESTS = 3


@router.post(
    "/factories/{factory_id}/quests/compose-support",
    response_model=QuestInstance,
    status_code=status.HTTP_201_CREATED,
)
def compose_support_quest(factory_id: str, payload: QuestContext) -> QuestInstance:
    """공장의 상태 스냅샷을 분석하여 최우선 순위의 달성 가능한 지원 퀘스트를 생성합니다.

    [데이터 흐름 및 필터링 정책]
    1. active 퀘스트 상한 검사: 공장당 최대 active 지원 퀘스트는 3개(MAX_ACTIVE_SUPPORT_QUESTS)로 제한합니다.
    2. 중복 아이템 검사: 이미 활성화된 퀘스트의 타겟 아이템(접두사 제거 정규화)과 동일한 아이템은 재생성을 차단합니다.
    3. Context 정규화 및 RuleGenerator 기반 초안 후보군 추출.
    4. QuestValidator를 거쳐 선행조건(feasibility)을 통과한 첫 번째 초안을 선택합니다.
    5. QuestManager 및 Repository를 통해 트랜잭션 범위 안에서 영속화합니다.
    """
    # TODO(auth): 호출자와 factory_id 간의 소유권 및 접근 권한 인가 검증 필요
    with get_db_session() as session:
        # 1. 활성 퀘스트 목록 조회 및 개수 제한 검사
        active_quests = QuestRepository.get_active_instances(session, factory_id)
        if len(active_quests) >= MAX_ACTIVE_SUPPORT_QUESTS:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=f"Active support quest limit exceeded (maximum {MAX_ACTIVE_SUPPORT_QUESTS})",
            )

        # 2. 이미 활성 중인 퀘스트의 타겟 아이템 ID 추출 (중복 차단 검사용)
        active_target_ids = set()
        for inst in active_quests:
            for obj in inst.objective_json:
                if isinstance(obj, dict) and "target_id" in obj:
                    active_target_ids.add(obj["target_id"])

        # 3. Context 정규화 및 부족 자원 계산
        context = QuestContextBuilder.build_context(payload.model_dump())

        # 4. 지원 퀘스트 초안 생성 후보군 추출
        drafts = QuestRuleGenerator.generate_drafts(context, active_target_ids)
        if not drafts:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="No candidate support quests could be generated (no shortages or all already active)",
            )

        # 5. 후보군 중 첫 번째로 검증을 통과한 초안 선택
        selected_draft = None
        for draft in drafts:
            validation = QuestValidator.validate(draft, context, active_target_ids)
            if validation.valid:
                selected_draft = draft
                break

        if not selected_draft:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="No valid support quest draft passed the feasibility conditions",
            )

        # 6. 인스턴스 생성 및 영속화
        related_main_quest_id = (
            payload.current_main_quest.quest_id if payload.current_main_quest else None
        )
        instance = QuestManager.create_quest_from_draft(
            session=session,
            factory_id=factory_id,
            draft=selected_draft,
            related_main_quest_id=related_main_quest_id,
        )
        return instance


@router.get(
    "/factories/{factory_id}/quests",
    response_model=list[QuestInstance],
)
def list_quests(factory_id: str) -> list[QuestInstance]:
    """해당 공장에 발급된 모든 상태의 퀘스트 목록을 조회합니다."""
    # TODO(auth): 호출자와 factory_id 간의 소유권 및 접근 권한 인가 검증 필요
    with get_db_session() as session:
        instances = QuestRepository.get_all_instances(session, factory_id)
        return [inst.to_pydantic() for inst in instances]


@router.post(
    "/factories/{factory_id}/quests/events",
    status_code=status.HTTP_204_NO_CONTENT,
)
def handle_factory_event(factory_id: str, event: ItemCollectedEvent) -> None:
    """공장에서 전달된 이벤트를 수신하여 매칭되는 퀘스트의 진행도를 멱등하게 스냅샷 갱신합니다.

    [에러 처리 및 가드]
    - 경로의 factory_id와 요청 바디의 event.factory_id가 불일치할 경우 400 Bad Request 에러를 던집니다.
    """
    # TODO(auth): 호출자와 factory_id 간의 소유권 및 접근 권한 인가 검증 필요
    if event.factory_id != factory_id:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Path factory_id does not match request body factory_id",
        )

    with get_db_session() as session:
        QuestProgressTracker.track_item_collected(
            session=session,
            event_id=event.event_id,
            factory_id=factory_id,
            item_id=event.item_id,
            current_total=event.current_total,
        )
