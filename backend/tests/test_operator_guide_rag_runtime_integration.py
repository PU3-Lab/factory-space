"""Tests for operator_guide RAG runtime integration."""

from __future__ import annotations

from dataclasses import dataclass

from agents.operator_guide.service import ManualQAService


@dataclass(frozen=True)
class FakeSubQuestionResult:
    question: str


@dataclass(frozen=True)
class FakeRagRuntimeResult:
    context_text: str
    is_multi_question: bool
    sub_question_results: list[FakeSubQuestionResult]
    metadata: dict[str, object]


class FakeRagRuntime:
    def __init__(self) -> None:
        self.calls: list[str] = []

    def retrieve(self, question: str) -> FakeRagRuntimeResult:
        self.calls.append(question)
        return FakeRagRuntimeResult(
            context_text=(
                "[SUB_QUESTION 1]\n"
                "question: What is a crusher?\n"
                "confidence: high\n"
                "context for crusher"
            ),
            is_multi_question=True,
            sub_question_results=[
                FakeSubQuestionResult(question="What is a crusher?"),
                FakeSubQuestionResult(question="how do I make iron ingot?"),
            ],
            metadata={
                "sub_question_count": 2,
                "max_sub_questions": 3,
                "truncated": False,
                "confidence_counts": {"high": 2, "medium": 0, "low": 0},
            },
        )


def test_service_includes_rag_context_in_prompt_when_runtime_is_available() -> None:
    rag_runtime = FakeRagRuntime()

    prompt = ManualQAService(rag_runtime=rag_runtime).build_prompt(
        "What is a crusher? And how do I make iron ingot?",
        topic="machine",
        sub_agent="operator_guide.machine_help",
    )

    assert rag_runtime.calls == ["What is a crusher? And how do I make iron ingot?"]
    assert "[RAG_RETRIEVAL_CONTEXT]" in prompt
    assert "context for crusher" in prompt
    assert "[RAG_RETRIEVAL_METADATA]" in prompt
    assert '"sub_question_count": 2' in prompt


def test_service_exposes_rag_retrieval_metadata() -> None:
    result = ManualQAService(rag_runtime=FakeRagRuntime()).answer(
        "What is a crusher? And how do I make iron ingot?"
    )

    metadata = result.to_metadata()

    assert metadata["retrieval"]["is_multi_question"] is True
    assert metadata["retrieval"]["sub_question_count"] == 2
    assert metadata["retrieval"]["confidence_counts"] == {
        "high": 2,
        "medium": 0,
        "low": 0,
    }
