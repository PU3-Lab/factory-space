"""Manual Q&A service for the operator_guide agent."""

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
    """Coordinate intent routing and CSV evidence building."""

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
        """Return a Manual Q&A proto answer for one question."""

        _ = context
        return self.build_prompt_context(question).result

    def build_prompt_context(self, question: str) -> ManualQAPromptContext:
        """Return classified CSV evidence for one question."""

        intent = self._question_classifier.classify(question)
        return self._context_builder.build(question, intent)

    def build_prompt(
        self,
        question: str,
        *,
        topic: str,
        sub_agent: str,
    ) -> str:
        """Return an LLM prompt grounded in matched CSV evidence."""

        prompt_context = self.build_prompt_context(question)
        return self._prompt_builder.build(
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
    """Build an operator_guide fallback result from the CSV Manual Q&A service."""

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
    """Build a CSV-grounded LLM prompt for an operator guide leaf agent."""

    return ManualQAService().build_prompt(
        question,
        topic=topic,
        sub_agent=sub_agent,
    )

