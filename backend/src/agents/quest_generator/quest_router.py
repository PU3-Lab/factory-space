from __future__ import annotations

import logging

from fastapi import APIRouter, Depends, HTTPException, status

from agents.quest_generator.context_builder import QuestContextBuilder
from agents.quest_generator.manager import QuestManager
from agents.quest_generator.models import (
    ItemCollectedEvent,
    QuestContext,
    QuestInstance,
)
from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
from agents.quest_generator.repository import QuestRepository
from agents.quest_generator.rule_generator import QuestRuleGenerator
from agents.quest_generator.tracker import QuestProgressTracker
from agents.quest_generator.validator import QuestValidator
from db.engine import get_db_session
from llm.adapter import create_llm_adapter
from llm.settings import LLMSettings

logger = logging.getLogger(__name__)
router = APIRouter()

# 디폴트 QuestPhraseRefiner 생성 및 의존성 주입 헬퍼 함수
_settings = LLMSettings.from_env()
_default_adapter = create_llm_adapter(_settings.default)
default_refiner = QuestPhraseRefiner(_default_adapter)


def get_phrase_refiner() -> QuestPhraseRefiner:
    """QuestPhraseRefiner 의존성을 주입하는 헬퍼 함수입니다."""
    return default_refiner


# 퀘스트 생성 및 처리를 위한 비즈니스 정책 상수
MAX_ACTIVE_SUPPORT_QUESTS = 3


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

        # 5단계. 문구 윤색 (LLM Polish) 및 수치 안전성 보장 (결정 ②, ③)
        final_draft = selected_draft
        try:
            refined_draft = phrase_refiner.refine(selected_draft, context)

            # 결정 ③ 핵심: LLM이 혹시라도 변조했을 수 있는 수치 정보(objectives, rewards)를 강제로 원본값 복원
            # phrase_refiner.refine 내부에서도 처리하지만, 이중 안전망으로 라우터에서도 강제 보장
            refined_draft = refined_draft.model_copy(
                update={
                    "objectives": selected_draft.objectives,
                    "rewards": selected_draft.rewards,
                    "support_type": selected_draft.support_type,
                    "quest_type": selected_draft.quest_type,
                }
            )

            # 6단계. 윤색된 draft 재검증
            revalidation = QuestValidator.validate(
                refined_draft, context, active_target_ids
            )
            if revalidation.valid:
                final_draft = refined_draft
            else:
                # 6단계 폴백: 재검증 실패 시 원본으로 폴백
                logger.warning(
                    "Refined draft failed revalidation (reason: %s). Falling back to original draft.",
                    revalidation.reason,
                )
        except Exception as exc:
            # 5단계 폴백: 윤색 중 예외 발생 시 원본으로 폴백
            logger.warning(
                "Failed during quest phrasing refinement: %s. Falling back to original draft.",
                exc,
            )

        # 7. 인스턴스 생성 및 영속화
        related_main_quest_id = (
            payload.current_main_quest.quest_id if payload.current_main_quest else None
        )
        instance = QuestManager.create_quest_from_draft(
            session=session,
            factory_id=factory_id,
            draft=final_draft,
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
