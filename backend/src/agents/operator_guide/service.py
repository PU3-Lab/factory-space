"""operator_guide agent의 Manual Q&A 흐름을 조율하는 service 모듈.

초보자용 설명:
    이 파일은 질문을 바로 답하지 않고, 먼저 질문 유형을 분류한 뒤 CSV 근거를 모은다.
    이후 prompt builder가 그 근거를 LLM 프롬프트로 만들 수 있게 연결한다.
"""

from __future__ import annotations

from typing import Any, Protocol

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.manual_context_builder import (
    ManualQAContextBuilder,
    ManualQAPromptContext,
)
from agents.operator_guide.prompt_builder import ManualQAPromptBuilder
from agents.operator_guide.question_classifier import ManualQAQuestionClassifier
from agents.operator_guide.schemas import ManualQAResult
from agents.operator_guide.session_memory import OPERATOR_GUIDE_RECENT_CONVERSATION_KEY


class ManualQARagRuntime(Protocol):
    """operator_guide service가 기대하는 RAG runtime의 최소 인터페이스.

    초보자용 설명:
        service는 실제 DB나 embedding provider를 직접 알 필요가 없다.
        대신 `retrieve(question)`을 호출하면 RAG 검색 결과를 돌려주는 객체만
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
    ) -> None:
        self._repository = repository or CsvManualQARepository()
        self._rag_runtime = rag_runtime or self._global_rag_runtime
        self._question_classifier = ManualQAQuestionClassifier(self._repository)
        self._context_builder = ManualQAContextBuilder(self._repository)
        self._prompt_builder = ManualQAPromptBuilder()

    def answer(
        self,
        question: str,
        context: dict[str, object] | None = None,
    ) -> ManualQAResult:
        """LLM 없이 CSV 기반 proto 답변 객체를 만든다."""

        return self.build_prompt_context(question, context=context).result

    def build_prompt_context(
        self,
        question: str,
        context: dict[str, object] | None = None,
    ) -> ManualQAPromptContext:
        """질문을 분류하고, 그 질문에 맞는 CSV evidence를 모은다."""

        intent = self._question_classifier.classify(question)
        prompt_context = self._context_builder.build(question, intent)
        recent_conversation = _recent_conversation(context)
        if self._rag_runtime is None:
            return ManualQAPromptContext(
                result=prompt_context.result,
                evidence=prompt_context.evidence,
                recent_conversation=recent_conversation,
            )

        rag_result = self._rag_runtime.retrieve(question)
        rag_metadata = _rag_metadata(rag_result)
        result = prompt_context.result.model_copy(update={"retrieval": rag_metadata})
        return ManualQAPromptContext(
            result=result,
            evidence=prompt_context.evidence,
            rag_context_text=str(getattr(rag_result, "context_text", "")),
            rag_metadata=rag_metadata,
            recent_conversation=recent_conversation,
        )

    def build_prompt(
        self,
        question: str,
        *,
        topic: str,
        sub_agent: str,
        context: dict[str, object] | None = None,
    ) -> str:
        """CSV evidence를 근거로 한 LLM용 단일 문자열 prompt를 만든다."""

        prompt_context = self.build_prompt_context(question, context=context)
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
    ) -> list[dict[str, str]]:
        """system prompt와 user prompt가 분리된 chat messages를 만든다."""

        prompt_context = self.build_prompt_context(question, context=context)
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
    result = ManualQAService().answer(question, context=context.metadata)
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
) -> str:
    """leaf agent가 사용할 CSV 근거 기반 LLM prompt를 만든다."""

    return ManualQAService().build_prompt(
        question,
        topic=topic,
        sub_agent=sub_agent,
        context=context,
    )


def build_manual_qa_prompt_messages(
    question: str,
    *,
    topic: str,
    sub_agent: str,
    context: dict[str, object] | None = None,
) -> list[dict[str, str]]:
    """leaf agent가 사용할 system/user chat messages를 만든다."""

    return ManualQAService().build_prompt_messages(
        question,
        topic=topic,
        sub_agent=sub_agent,
        context=context,
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
