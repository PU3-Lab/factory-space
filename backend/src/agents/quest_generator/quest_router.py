from __future__ import annotations

import logging

from fastapi import APIRouter, Depends, HTTPException, status

from agents.quest_generator.compose_service import (
    MAX_ACTIVE_SUPPORT_QUESTS,
    compose_first_support_quest,
    get_phrase_refiner,
)
from agents.quest_generator.models import (
    ItemCollectedEvent,
    QuestContext,
    QuestInstance,
)
from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
from agents.quest_generator.repository import QuestRepository
from agents.quest_generator.tracker import QuestProgressTracker
from db.engine import get_db_session

logger = logging.getLogger(__name__)
router = APIRouter()


@router.post(
    "/factories/{factory_id}/quests/compose-support",
    response_model=QuestInstance,
    status_code=status.HTTP_201_CREATED,
)
def compose_support_quest(
    factory_id: str,
    payload: QuestContext,
    phrase_refiner: QuestPhraseRefiner = Depends(get_phrase_refiner),
) -> QuestInstance:
    """공장의 상태 스냅샷을 분석하여 최우선 순위의 달성 가능한 지원 퀘스트를 생성합니다.

    [데이터 흐름 및 필터링 정책]
    1. active 퀘스트 상한 검사: 공장당 최대 active 지원 퀘스트는 3개(MAX_ACTIVE_SUPPORT_QUESTS)로 제한합니다.
    2. 중복 아이템 검사: 이미 활성화된 퀘스트의 타겟 아이템(접두사 제거 정규화)과 동일한 아이템은 재생성을 차단합니다.
    3. Context 정규화 및 부족 자원 계산.
    4. QuestValidator를 거쳐 선행조건(feasibility)을 통과한 첫 번째 초안을 선택합니다 (selected_draft).
    5. ★신규★ QuestPhraseRefiner를 통해 title/description 문구를 다듬은 refined_draft를 생성합니다.
       - 실패 시 / 예외 발생 시 원본 selected_draft를 그대로 사용합니다.
       - LLM이 목표(objectives)나 보상(rewards)을 수정하려 하더라도, 구조적으로 원본 selected_draft의 값으로 강제 고정합니다.
    6. ★신규★ QuestValidator를 사용하여 refined_draft에 대해 재검증을 수행합니다.
       - 만약 재검증이 실패할 경우, 안전하게 원본 selected_draft로 폴백합니다.
    7. QuestManager 및 Repository를 통해 트랜잭션 범위 안에서 영속화합니다.
    """
    # TODO(auth): 호출자와 factory_id 간의 소유권 및 접근 권한 인가 검증 필요
    with get_db_session() as session:
        result = compose_first_support_quest(
            session=session,
            factory_id=factory_id,
            context_payload=payload,
            phrase_refiner=phrase_refiner,
        )

        if result.outcome == "none":
            if result.reason == "limit_exceeded":
                raise HTTPException(
                    status_code=status.HTTP_400_BAD_REQUEST,
                    detail=f"Active support quest limit exceeded (maximum {MAX_ACTIVE_SUPPORT_QUESTS})",
                )
            elif result.reason == "no_candidates":
                raise HTTPException(
                    status_code=status.HTTP_400_BAD_REQUEST,
                    detail="No candidate support quests could be generated (no shortages or all already active)",
                )
            elif result.reason == "no_valid_draft":
                raise HTTPException(
                    status_code=status.HTTP_400_BAD_REQUEST,
                    detail="No valid support quest draft passed the feasibility conditions",
                )
            else:
                raise HTTPException(
                    status_code=status.HTTP_400_BAD_REQUEST,
                    detail=result.reason or "Unknown error",
                )

        if result.instance is None:
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="Failed to retrieve composed quest instance",
            )
        return result.instance


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
