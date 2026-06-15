"""여러 sub-question에 대해 RAG 검색을 순서대로 실행하는 모듈.

초보자용 설명:
    `Question Decomposer`는 긴 질문을 작은 질문들로 나눈다.
    이 모듈은 나뉜 질문 하나하나를 기존 `ManualRagRetriever`에 전달해
    각각 검색하고, LLM prompt에 넣기 쉬운 하나의 context로 다시 묶는다.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from agents.operator_guide.question_decomposer import decompose_question
from agents.operator_guide.rag_retriever import ManualRagRetrievalResult


class RagRetriever(Protocol):
    """sub-question 하나를 RAG 검색할 수 있는 객체의 최소 약속."""

    def retrieve(self, query: str) -> ManualRagRetrievalResult:
        """질문 하나를 검색하고 retrieval 결과를 반환한다."""


@dataclass(frozen=True)
class SubQuestionRagResult:
    """sub-question 하나와 그 질문에 대한 RAG 검색 결과."""

    index: int
    question: str
    retrieval: ManualRagRetrievalResult


@dataclass(frozen=True)
class MultiQuestionRagResult:
    """원본 질문과 sub-question별 RAG 검색 결과를 묶은 값 객체."""

    original_question: str
    is_multi_question: bool
    sub_question_results: list[SubQuestionRagResult]
    context_text: str
    metadata: dict[str, object]


class MultiQuestionRagRetriever:
    """질문 분해 결과를 이용해 sub-question별 RAG 검색을 실행한다."""

    def __init__(self, *, rag_retriever: RagRetriever) -> None:
        self._rag_retriever = rag_retriever

    def retrieve(self, question: str) -> MultiQuestionRagResult:
        """원본 질문을 분해하고 각 sub-question마다 RAG 검색을 수행한다."""

        decomposition = decompose_question(question)
        sub_question_results = [
            SubQuestionRagResult(
                index=sub_question.index,
                question=sub_question.question,
                retrieval=self._rag_retriever.retrieve(sub_question.question),
            )
            for sub_question in decomposition.sub_questions
        ]
        return MultiQuestionRagResult(
            original_question=decomposition.original_question,
            is_multi_question=decomposition.is_multi_question,
            sub_question_results=sub_question_results,
            context_text=_build_combined_context(sub_question_results),
            metadata={
                **decomposition.metadata,
                "confidence_counts": _count_confidence(sub_question_results),
            },
        )


def _build_combined_context(results: list[SubQuestionRagResult]) -> str:
    parts = [
        "\n".join(
            [
                f"[SUB_QUESTION {result.index}]",
                f"question: {result.question}",
                f"confidence: {result.retrieval.confidence}",
                result.retrieval.context_text,
            ],
        )
        for result in results
    ]
    return "\n\n".join(parts)


def _count_confidence(
    results: list[SubQuestionRagResult],
) -> dict[str, int]:
    counts = {"high": 0, "medium": 0, "low": 0}
    for result in results:
        counts[result.retrieval.confidence] += 1
    return counts
