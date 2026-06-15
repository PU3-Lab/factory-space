"""operator_guide agent의 Manual Q&A 흐름을 조율하는 service 모듈.

초보자용 설명:
    이 파일은 질문을 바로 답하지 않고, 먼저 질문 유형을 분류한 뒤 CSV 근거를 모은다.
    이후 prompt builder가 그 근거를 LLM 프롬프트로 만들 수 있게 연결한다.
"""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.manual_context_builder import (
    ManualQAContextBuilder,
    ManualQAPromptContext,
)
from agents.operator_guide.prompt_builder import ManualQAPromptBuilder
from agents.operator_guide.question_classifier import ManualQAQuestionClassifier
from agents.operator_guide.schemas import ManualQAResult


class ManualQAService:
    """질문 분류, CSV 근거 수집, prompt 생성을 한 곳에서 조율한다."""

    def __init__(
        self,
        repository: CsvManualQARepository | None = None,
    ) -> None:
        self._repository = repository or CsvManualQARepository()
        self._question_classifier = ManualQAQuestionClassifier(self._repository)
        self._context_builder = ManualQAContextBuilder(self._repository)
        self._prompt_builder = ManualQAPromptBuilder()

    def answer(
        self,
        question: str,
        context: dict[str, object] | None = None,
    ) -> ManualQAResult:
        """LLM 없이 CSV 기반 proto 답변 객체를 만든다."""

        _ = context
        return self.build_prompt_context(question).result

    def build_prompt_context(self, question: str) -> ManualQAPromptContext:
        """질문을 분류하고, 그 질문에 맞는 CSV evidence를 모은다."""

        intent = self._question_classifier.classify(question)
        return self._context_builder.build(question, intent)

    def build_prompt(
        self,
        question: str,
        *,
        topic: str,
        sub_agent: str,
    ) -> str:
        """CSV evidence를 근거로 한 LLM용 단일 문자열 prompt를 만든다."""

        prompt_context = self.build_prompt_context(question)
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
    ) -> list[dict[str, str]]:
        """system prompt와 user prompt가 분리된 chat messages를 만든다."""

        prompt_context = self.build_prompt_context(question)
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
) -> str:
    """leaf agent가 사용할 CSV 근거 기반 LLM prompt를 만든다."""

    return ManualQAService().build_prompt(
        question,
        topic=topic,
        sub_agent=sub_agent,
    )


def build_manual_qa_prompt_messages(
    question: str,
    *,
    topic: str,
    sub_agent: str,
) -> list[dict[str, str]]:
    """leaf agent가 사용할 system/user chat messages를 만든다."""

    return ManualQAService().build_prompt_messages(
        question,
        topic=topic,
        sub_agent=sub_agent,
    )
