"""공통 지원 퀘스트 생성 파이프라인 서비스입니다.

초보자용 설명:
    REST API 라우터와 WebSocket 핸들러 양쪽에서 공통으로 호출하여 규칙 기반 지원 퀘스트를 생성합니다.
    공장의 부족한 자원을 판별한 뒤 최적의 지원 퀘스트 초안을 찾고, LLM을 거쳐 최종 데이터베이스에 저장합니다.
"""

from __future__ import annotations

import logging
from typing import Literal

from pydantic import BaseModel
from sqlalchemy.orm import Session

from agents.quest_generator.context_builder import QuestContextBuilder
from agents.quest_generator.manager import QuestManager
from agents.quest_generator.models import QuestContext, QuestInstance
from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
from agents.quest_generator.repository import QuestRepository
from agents.quest_generator.rule_generator import QuestRuleGenerator
from agents.quest_generator.validator import QuestValidator
from llm.adapter import create_llm_adapter
from llm.settings import LLMSettings

logger = logging.getLogger(__name__)

# 디폴트 QuestPhraseRefiner 생성 및 의존성 주입 헬퍼 함수
# 초보자용 설명:
#   default_refiner는 내부 상태를 유지하지 않는(stateless) LLM 어댑터를 감싸고 있습니다.
#   따라서 REST API(스레드풀)와 WebSocket(asyncio.to_thread) 등 멀티스레드 환경에서
#   여러 스레드가 동시에 이 싱글톤 인스턴스에 접근하더라도 데이터 오염이나 경합 없이
#   안전하게 동작(Thread-safe)합니다.
_settings = LLMSettings.from_env()
_default_adapter = create_llm_adapter(_settings.default)
default_refiner = QuestPhraseRefiner(_default_adapter)


def get_phrase_refiner() -> QuestPhraseRefiner:
    """QuestPhraseRefiner 의존성을 주입하는 헬퍼 함수입니다."""
    return default_refiner


# 지원 퀘스트 생성 및 처리를 위한 비즈니스 정책 상수
MAX_ACTIVE_SUPPORT_QUESTS = 3


class ComposeResult(BaseModel):
    """지원 퀘스트 생성 결과를 나타내는 스키마 클래스입니다.

    속성:
        outcome: 생성 성공 여부 ("composed" 또는 "none")
        instance: 생성 완료된 퀘스트의 DB 인스턴스 (성공 시)
        reason: 생성 불가 시 그 사유 ("limit_exceeded", "no_candidates", "no_valid_draft")
    """

    outcome: Literal["composed", "none"]
    instance: QuestInstance | None = None
    reason: Literal["limit_exceeded", "no_candidates", "no_valid_draft"] | None = None


def compose_first_support_quest(
    session: Session,
    factory_id: str,
    context_payload: dict | QuestContext,
    phrase_refiner: QuestPhraseRefiner,
) -> ComposeResult:
    """공장의 상태 스냅샷을 분석하여 최우선 순위의 달성 가능한 지원 퀘스트를 생성합니다.

    초보자용 설명:
        이 함수는 지원 퀘스트 생성에 필요한 검사 흐름을 선형적으로 처리하는 비즈니스 로직의 중심점입니다.
        1. 활성화된 지원 퀘스트 개수를 조회해 제한선(3개)을 넘었는지 확인합니다.
        2. 입력된 context_payload 형식을 파악해 QuestContext 객체로 변환 또는 정규화합니다.
        3. 중복 생성을 막기 위해 이미 활성화된 퀘스트 대상 아이템을 제외한 부족 자원(Known Issue)을 순회합니다.
        4. Validator로 제작 경로가 유효한지 검증해 첫 번째 유효 초안을 골라냅니다.
        5. AI(phrase_refiner)를 통해 몰입감을 더해줄 한국어 제목과 설명으로 윤색합니다.
        6. 수치와 중요 타겟 정보가 AI에 의해 훼손되지 않도록 강제 복원(Guard)하고 재검증을 진행합니다.
        7. 최종 결정된 초안을 기반으로 QuestManager를 통해 DB에 저장하고, ComposeResult 형태로 반환합니다.
    """
    # 1. 활성 퀘스트 목록 조회 및 개수 제한 검사
    active_quests = QuestRepository.get_active_instances(session, factory_id)
    if len(active_quests) >= MAX_ACTIVE_SUPPORT_QUESTS:
        return ComposeResult(outcome="none", reason="limit_exceeded")

    # 2. 이미 활성 중인 퀘스트의 타겟 아이템 ID 추출 (중복 차단 검사용)
    active_target_ids = set()
    for inst in active_quests:
        for obj in inst.objective_json:
            if isinstance(obj, dict) and "target_id" in obj:
                active_target_ids.add(obj["target_id"])

    # 3. Context 정규화 및 부족 자원 계산
    if isinstance(context_payload, QuestContext):
        context = QuestContextBuilder.build_context(context_payload.model_dump())
    else:
        context = QuestContextBuilder.build_context(context_payload)

    # 4. 지원 퀘스트 초안 생성 후보군 추출
    drafts = QuestRuleGenerator.generate_drafts(context, active_target_ids)
    if not drafts:
        return ComposeResult(outcome="none", reason="no_candidates")

    # 5. 후보군 중 첫 번째로 검증을 통과한 초안 선택
    selected_draft = None
    for draft in drafts:
        validation = QuestValidator.validate(draft, context, active_target_ids)
        if validation.valid:
            selected_draft = draft
            break

    if not selected_draft:
        return ComposeResult(outcome="none", reason="no_valid_draft")

    # 5단계. 문구 윤색 (LLM Polish) 및 수치 안전성 보장
    final_draft = selected_draft
    try:
        refined_draft = phrase_refiner.refine(selected_draft, context)

        # LLM이 혹시라도 변조했을 수 있는 수치 정보(objectives, rewards)를 강제로 원본값 복원
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
            # 재검증 실패 시 원본으로 폴백
            logger.warning(
                "Refined draft failed revalidation (reason: %s). Falling back to original draft.",
                revalidation.reason,
            )
    except Exception as exc:
        # 윤색 중 예외 발생 시 원본으로 폴백
        logger.warning(
            "Failed during quest phrasing refinement: %s. Falling back to original draft.",
            exc,
        )

    # 7. 인스턴스 생성 및 영속화
    related_main_quest_id = (
        context.current_main_quest.quest_id if context.current_main_quest else None
    )
    instance = QuestManager.create_quest_from_draft(
        session=session,
        factory_id=factory_id,
        draft=final_draft,
        related_main_quest_id=related_main_quest_id,
    )
    return ComposeResult(outcome="composed", instance=instance)
