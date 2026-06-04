"""Manual Q&A service for the operator_guide agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.question_classifier import ManualQAQuestionClassifier
from agents.operator_guide.response_builder import ManualQAResponseBuilder
from agents.operator_guide.schemas import ManualQAResult


class ManualQAService:
    """Coordinate intent routing, CSV lookup, and template answer building."""

    def __init__(
        self,
        repository: CsvManualQARepository | None = None,
    ) -> None:
        self._repository = repository or CsvManualQARepository()
        self._question_classifier = ManualQAQuestionClassifier(self._repository)
        self._response_builder = ManualQAResponseBuilder(self._repository)

    def answer(
        self,
        question: str,
        context: dict[str, object] | None = None,
    ) -> ManualQAResult:
        """Return a Manual Q&A proto answer for one question."""

        _ = context
        intent = self._question_classifier.classify(question)
        return self._response_builder.build(question, intent)


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

