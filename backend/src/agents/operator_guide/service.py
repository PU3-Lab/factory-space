"""operator_guide agent의 Manual Q&A 흐름을 조율하는 service 모듈.

초보자용 설명:
    이 파일은 질문을 바로 답하지 않고, 먼저 질문 유형을 분류한 뒤 CSV 근거를 모은다.
    이후 prompt builder가 그 근거를 LLM 프롬프트로 만들 수 있게 연결한다.
"""

from __future__ import annotations

import logging
from collections.abc import Callable
from typing import Any, Protocol

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.manual_context_builder import (
    ManualQAContextBuilder,
    ManualQAPromptContext,
)
from agents.operator_guide.prompt_builder import ManualQAPromptBuilder
from agents.operator_guide.question_classifier import (
    ContextNeedClassifier,
    ManualQAQuestionClassifier,
)
from agents.operator_guide.schemas import ManualQAResult
from agents.operator_guide.session_memory import OPERATOR_GUIDE_RECENT_CONVERSATION_KEY
from llm.adapter import LLMAdapter

LOGGER = logging.getLogger(__name__)

# 진행 상태 메시지 카탈로그 (Progress Message Catalog)
# 초보자용 설명:
#     플레이어가 질문 유형에 따라 느끼는 경험(UX)을 극대화하기 위해,
#     단계별로 에이전트가 어떤 작업을 하고 있는지 상태 메시지를 사전 정의한 테이블입니다.
PROGRESS_CATALOG: dict[str, list[tuple[str, str]]] = {
    "machine": [
        ("rag_search", "장비 매뉴얼을 확인하는 중입니다..."),
        ("state_check", "입력과 출력 자원을 살펴보는 중입니다..."),
        ("logic_format", "플레이어가 이해하기 쉽게 정리하는 중입니다..."),
    ],
    "recipe": [
        ("rag_search", "관련 레시피 매뉴얼을 확인하는 중입니다..."),
        ("state_check", "필요한 재료와 장비를 살펴보는 중입니다..."),
        ("logic_format", "생산 흐름을 이해하기 쉽게 정리하는 중입니다..."),
    ],
    "troubleshooting": [
        ("rag_search", "문제 상황과 관련된 매뉴얼을 찾는 중입니다..."),
        ("state_check", "선택된 장비 상태를 확인하는 중입니다..."),
        ("power_check", "전력과 입력 자원 상태를 대조하는 중입니다..."),
        ("document_find", "관련 문제 해결 절차를 확인하는 중입니다..."),
        ("step_arrange", "점검 순서를 정리하는 중입니다..."),
    ],
}


class CurrentGameStateTool:
    """플레이어가 제공한 실시간 게임 상태 컨텍스트에서 필요한 범위(Scope)의 정보를 필터링하고 문자열로 직렬화하는 도구입니다.

    초보자용 설명:
        LLM에게 공장의 실시간 정보를 그대로 부으면 토큰 낭비가 심하므로,
        질문 분석을 통해 도출된 Scope(예: powerStatus, inputInventory)에 해당하는 데이터만
        추출하고 사용하기 쉬운 텍스트(예: "Power Status: OFF")로 가공하여 돌려줍니다.
    """

    def extract_state(
        self,
        raw_state: dict[str, Any] | None,
        scopes: list[str],
    ) -> tuple[str, list[str]]:
        """원시 게임 상태 딕셔너리에서 요청받은 scopes만 선별하여 프롬프트 삽입용 문자열과 실제 수집된 scopes 목록을 반환합니다."""

        if not raw_state:
            return "", []

        lines: list[str] = []
        available_scopes: list[str] = []
        for scope in scopes:
            if scope in raw_state:
                val = raw_state[scope]
                lines.append(f"{scope}: {val}")
                available_scopes.append(scope)

        return "\n".join(lines), available_scopes


class ManualQARagRuntime(Protocol):
    """operator_guide service가 기대하는 RAG runtime의 최소 인터페이스.

    초보자용 설명:
        service는 실제 DB나 embedding provider를 직접 알 필요가 없다.
        대신 retrieve(question)을 호출하면 RAG 검색 결과를 돌려주는 객체만
        받는다. 이렇게 하면 테스트에서는 fake runtime을 넣고, 실제 실행에서는
        PostgreSQL/pgvector 기반 runtime을 넣을 수 있다.
    """

    def retrieve(self, question: str) -> object:
        """플레이어 질문을 받아 RAG 검색 결과를 반환한다."""


class ManualQAService:
    """질문 분류, CSV 근거 수집, prompt 생성을 한 곳에서 조율한다."""

    _global_rag_runtime: ManualQARagRuntime | None = None

    @classmethod
    def set_global_rag_runtime(cls, rag_runtime: ManualQARagRuntime | None) -> None:
        """글로벌 RAG 런타임 인스턴스를 설정한다."""
        cls._global_rag_runtime = rag_runtime

    def __init__(
        self,
        repository: CsvManualQARepository | None = None,
        rag_runtime: ManualQARagRuntime | None = None,
        llm_adapter: LLMAdapter | None = None,
    ) -> None:
        self._repository = repository or CsvManualQARepository()
        self._rag_runtime = rag_runtime or self._global_rag_runtime
        self._question_classifier = ManualQAQuestionClassifier(self._repository)
        self._need_classifier = ContextNeedClassifier(llm_adapter)
        self._game_state_tool = CurrentGameStateTool()
        self._context_builder = ManualQAContextBuilder(self._repository)
        self._prompt_builder = ManualQAPromptBuilder()

    def answer(
        self,
        question: str,
        context: dict[str, object] | None = None,
        topic: str | None = None,
        on_progress: Callable[[str, str], None] | None = None,
    ) -> ManualQAResult:
        """LLM 없이 CSV 기반 proto 답변 객체를 만든다."""

        return self.build_prompt_context(
            question, context=context, topic=topic, on_progress=on_progress
        ).result

    def build_prompt_context(
        self,
        question: str,
        context: dict[str, object] | None = None,
        topic: str | None = None,
        on_progress: Callable[[str, str], None] | None = None,
    ) -> ManualQAPromptContext:
        """질문을 분류하고, 그 질문에 맞는 CSV evidence를 모은다."""

        # 1단계: RAG 검색 진행 중 메시지 전송
        if on_progress and topic in PROGRESS_CATALOG:
            on_progress(PROGRESS_CATALOG[topic][0][0], PROGRESS_CATALOG[topic][0][1])

        intent = self._question_classifier.classify(question, context)
        prompt_context = self._context_builder.build(question, intent)
        recent_conversation = _recent_conversation(context)
        confirmed_facts = _confirmed_facts(context)
        response_style = _response_style(context)

        # 2단계: 실시간 게임 상태 확인 진행 중 메시지 전송
        if on_progress and topic in PROGRESS_CATALOG:
            on_progress(PROGRESS_CATALOG[topic][1][0], PROGRESS_CATALOG[topic][1][1])

        # Game State 관련 분석 및 추출
        requires_state, required_scopes = self._need_classifier.classify_need(
            question, intent.question_type
        )
        raw_state = None
        if context is not None:
            raw_state = context.get("current_game_state")
            if not isinstance(raw_state, dict):
                raw_state = None

        state_text = ""
        available_scopes = []
        used_state = False

        if requires_state and raw_state:
            state_text, available_scopes = self._game_state_tool.extract_state(
                raw_state, required_scopes
            )
            used_state = bool(state_text)

        # 3단계: 문제해결(troubleshooting) 특화 진행 메시지 스트리밍
        if topic == "troubleshooting" and on_progress:
            on_progress("power_check", "전력과 입력 자원 상태를 대조하는 중입니다...")
            on_progress("document_find", "관련 문제 해결 절차를 확인하는 중입니다...")

        def build_csv_only_context() -> ManualQAPromptContext:
            result = prompt_context.result.model_copy(
                update={
                    "requires_current_game_state": requires_state,
                    "used_current_game_state": used_state,
                    "required_state_scopes": required_scopes,
                    "available_scopes": available_scopes,
                }
            )
            # 최종 정돈 진행 메시지 전송
            if on_progress and topic in PROGRESS_CATALOG:
                on_progress(
                    PROGRESS_CATALOG[topic][-1][0], PROGRESS_CATALOG[topic][-1][1]
                )

            return ManualQAPromptContext(
                result=result,
                evidence=prompt_context.evidence,
                recent_conversation=recent_conversation,
                confirmed_facts=confirmed_facts,
                current_game_state_text=state_text,
                requires_current_game_state=requires_state,
                used_current_game_state=used_state,
                required_state_scopes=required_scopes,
                available_scopes=available_scopes,
                response_style=response_style,
            )

        if self._rag_runtime is None:
            return build_csv_only_context()

        # 이미 동일 요청 내에서 RAG 검색이 실행되었는지 캐시를 확인합니다.
        # 초보자용 설명:
        #     동일 턴 내에서 prompt string 빌드와 prompt messages 빌드가 순차적으로 실행되는데,
        #     매번 RAG 검색을 실행하면 중복 호출로 비용과 속도가 저하됩니다.
        #     따라서 context 딕셔너리에 이전에 검색한 결과를 캐싱하여 한 번만 호출되도록 합니다.
        if context is not None and "_cached_rag_result" in context:
            rag_result = context["_cached_rag_result"]
        else:
            # confirmed facts를 RAG 검색 질문에 병합하여 검색 품질을 개선합니다.
            search_query = question
            if confirmed_facts:
                search_query = f"{question} {' '.join(confirmed_facts)}"

            try:
                rag_result = self._rag_runtime.retrieve(search_query)
            except Exception as exc:
                LOGGER.warning(
                    "operator_guide RAG retrieval failed; falling back to CSV-only context: %s",
                    exc,
                )
                rag_result = None
            if context is not None:
                context["_cached_rag_result"] = rag_result

        if rag_result is None:
            return build_csv_only_context()

        rag_metadata = _rag_metadata(rag_result)
        result = prompt_context.result.model_copy(
            update={
                "retrieval": rag_metadata,
                "requires_current_game_state": requires_state,
                "used_current_game_state": used_state,
                "required_state_scopes": required_scopes,
                "available_scopes": available_scopes,
            }
        )
        # 최종 정돈 진행 메시지 전송
        if on_progress and topic in PROGRESS_CATALOG:
            on_progress(PROGRESS_CATALOG[topic][-1][0], PROGRESS_CATALOG[topic][-1][1])

        return ManualQAPromptContext(
            result=result,
            evidence=prompt_context.evidence,
            rag_context_text=str(getattr(rag_result, "context_text", "")),
            rag_metadata=rag_metadata,
            recent_conversation=recent_conversation,
            confirmed_facts=confirmed_facts,
            current_game_state_text=state_text,
            requires_current_game_state=requires_state,
            used_current_game_state=used_state,
            required_state_scopes=required_scopes,
            available_scopes=available_scopes,
            response_style=response_style,
        )

    def build_prompt(
        self,
        question: str,
        *,
        topic: str,
        sub_agent: str,
        context: dict[str, object] | None = None,
        on_progress: Callable[[str, str], None] | None = None,
    ) -> str:
        """CSV evidence를 근거로 한 LLM용 단일 문자열 prompt를 만든다."""

        prompt_context = self.build_prompt_context(
            question, context=context, topic=topic, on_progress=on_progress
        )
        return self._prompt_builder.build(
            question=question,
            topic=topic,
            sub_agent=sub_agent,
            context=prompt_context,
        )

    def build_prompt_messages(
        self,
        question: str,
        *,
        topic: str,
        sub_agent: str,
        context: dict[str, object] | None = None,
        on_progress: Callable[[str, str], None] | None = None,
    ) -> list[dict[str, str]]:
        """system prompt와 user prompt가 분리된 chat messages를 만든다."""

        prompt_context = self.build_prompt_context(
            question, context=context, topic=topic, on_progress=on_progress
        )
        return self._prompt_builder.build_messages(
            question=question,
            topic=topic,
            sub_agent=sub_agent,
            context=prompt_context,
        )


def build_manual_qa_agent_result(
    payload: dict[str, Any],
    context: AgentContext,
    *,
    topic: str,
    sub_agent: str,
) -> AgentRunResult:
    """LLM 호출 실패 시 사용할 operator_guide fallback 응답을 만든다."""

    question = str(payload.get("question") or payload.get("message") or "")
    result = ManualQAService().answer(
        question,
        context=context.metadata,
        topic=topic,
        on_progress=getattr(context, "on_progress", None),
    )
    return AgentRunResult(
        agent="operator_guide",
        payload={
            "final_answer": result.final_answer,
            "actions": [],
            "question": question,
            "topic": topic,
        },
        metadata={
            **result.to_metadata(),
            "fallback": True,
            "sub_agent": sub_agent,
        },
    )


def build_manual_qa_prompt(
    question: str,
    *,
    topic: str,
    sub_agent: str,
    context: dict[str, object] | None = None,
    on_progress: Callable[[str, str], None] | None = None,
) -> str:
    """leaf agent가 사용할 CSV 근거 기반 LLM prompt를 만든다."""

    return ManualQAService().build_prompt(
        question,
        topic=topic,
        sub_agent=sub_agent,
        context=context,
        on_progress=on_progress,
    )


def build_manual_qa_prompt_messages(
    question: str,
    *,
    topic: str,
    sub_agent: str,
    context: dict[str, object] | None = None,
    on_progress: Callable[[str, str], None] | None = None,
) -> list[dict[str, str]]:
    """leaf agent가 사용할 system/user chat messages를 만든다."""

    return ManualQAService().build_prompt_messages(
        question,
        topic=topic,
        sub_agent=sub_agent,
        context=context,
        on_progress=on_progress,
    )


def _recent_conversation(context: dict[str, object] | None) -> list[dict[str, str]]:
    """AgentContext metadata에서 최근 대화 목록만 안전하게 꺼낸다.

    초보자용 설명:
        pipeline은 session memory를 metadata에 넣어 service로 넘깁니다.
        이 함수는 그 값이 예상한 list/dict 형태인지 확인하고, prompt에 넣을 수 있는
        question/answer 문자열만 골라냅니다.
    """

    if context is None:
        return []

    raw_turns = context.get(OPERATOR_GUIDE_RECENT_CONVERSATION_KEY)
    if not isinstance(raw_turns, list):
        return []

    turns: list[dict[str, str]] = []
    for raw_turn in raw_turns:
        if not isinstance(raw_turn, dict):
            continue
        question = str(raw_turn.get("question") or "")
        answer = str(raw_turn.get("answer") or "")
        if question and answer:
            turns.append({"question": question, "answer": answer})
    return turns


def _confirmed_facts(context: dict[str, object] | None) -> list[str]:
    """AgentContext metadata에서 플레이어가 확인해 준 사실 목록을 꺼냅니다.

    초보자용 설명:
        pipeline은 session memory에서 플레이어가 대화 중 직접 확인해 준 사실
        목록을 context에 넣어 service로 보냅니다. 이 함수는 그 값이 list 형태인지
        체크하고 안전하게 꺼내 줍니다.
    """

    if context is None:
        return []

    raw_facts = context.get("confirmed_facts")
    if not isinstance(raw_facts, list):
        return []

    return [str(fact) for fact in raw_facts if fact]


def _response_style(context: dict[str, object] | None) -> str:
    """AgentContext metadata에서 답변 길이 스타일을 안전하게 꺼냅니다."""

    if context is None:
        return "normal"

    raw_style = context.get("response_style")
    if raw_style is None:
        return "normal"

    style = str(raw_style).strip().lower()
    if style in {"short", "normal", "detailed"}:
        return style
    return "normal"


def _rag_metadata(rag_result: object) -> dict[str, object]:
    raw_metadata = dict(getattr(rag_result, "metadata", {}) or {})
    sub_question_results = list(getattr(rag_result, "sub_question_results", []) or [])
    sub_questions = [
        {
            "index": getattr(sub_question_result, "index", index),
            "question": getattr(sub_question_result, "question", ""),
        }
        for index, sub_question_result in enumerate(sub_question_results, start=1)
    ]
    return {
        "is_multi_question": bool(getattr(rag_result, "is_multi_question", False)),
        "sub_question_count": raw_metadata.get(
            "sub_question_count",
            len(sub_question_results),
        ),
        "max_sub_questions": raw_metadata.get("max_sub_questions"),
        "truncated": raw_metadata.get("truncated", False),
        "confidence_counts": raw_metadata.get(
            "confidence_counts",
            {"high": 0, "medium": 0, "low": 0},
        ),
        "sub_questions": sub_questions,
    }
