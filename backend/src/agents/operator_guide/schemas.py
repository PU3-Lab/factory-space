"""Schemas for the CSV-backed Manual Q&A proto."""

from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, computed_field

QuestionType = Literal[
    "equipment_question",
    "resource_question",
    "recipe_question",
    "troubleshooting_question",
    "unknown_question",
]


class QAChatbotPayload(BaseModel):
    """Payload accepted by the Q&A chatbot agent."""

    model_config = ConfigDict(extra="forbid")

    question: str = Field(min_length=1)
    context: dict[str, object] = Field(default_factory=dict)


class ManualQAIntent(BaseModel):
    """Question classification result used inside the operator_guide agent."""

    model_config = ConfigDict(extra="forbid")

    question_type: QuestionType
    primary_manual: str
    supporting_manuals: list[str] = Field(default_factory=list)
    target_ids: list[str] = Field(default_factory=list)


class ManualQASource(BaseModel):
    """Source row used to build a Manual Q&A answer."""

    model_config = ConfigDict(extra="forbid")

    doc_id: str
    type: str
    title: str


class RecommendedAction(BaseModel):
    """Action recommendation shown as metadata, not an Unreal command."""

    model_config = ConfigDict(extra="forbid")

    action_id: str
    label: str
    description: str
    priority: int


class ManualQAResult(BaseModel):
    """Structured result returned by the Manual Q&A service."""

    model_config = ConfigDict(extra="forbid")

    question: str
    question_type: QuestionType
    answer: str
    sources: list[ManualQASource] = Field(default_factory=list)
    recommended_actions: list[RecommendedAction] = Field(default_factory=list)
    confidence: Literal["high", "medium", "low"]
    primary_manual: str
    supporting_manuals: list[str] = Field(default_factory=list)
    target_ids: list[str] = Field(default_factory=list)

    @computed_field
    @property
    def final_answer(self) -> str:
        """Final user-facing answer kept stable across proto/alpha/beta/final."""

        return self.answer

    def to_metadata(self) -> dict[str, object]:
        """Return the public metadata shape used by Manual Q&A responses."""

        return {
            "question": self.question,
            "question_type": self.question_type,
            "sources": [source.model_dump() for source in self.sources],
            "recommended_actions": [
                action.model_dump() for action in self.recommended_actions
            ],
            "confidence": self.confidence,
            "primary_manual": self.primary_manual,
            "supporting_manuals": self.supporting_manuals,
            "target_ids": self.target_ids,
        }
