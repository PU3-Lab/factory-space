"""Manual Q&A service for the operator_guide agent."""

from __future__ import annotations

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

