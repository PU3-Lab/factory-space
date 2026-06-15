"""Tests for operator_guide multi-question RAG retrieval."""

from __future__ import annotations

from agents.operator_guide.multi_question_rag_retriever import (
    MultiQuestionRagRetriever,
)
from agents.operator_guide.rag_retriever import (
    ManualRagRetrievalResult,
    ManualRagSearchResult,
)


class FakeRagRetriever:
    def __init__(self) -> None:
        self.calls: list[str] = []

    def retrieve(self, query: str) -> ManualRagRetrievalResult:
        self.calls.append(query)
        return _retrieval_result(query)


def test_multi_question_retriever_searches_each_sub_question() -> None:
    rag_retriever = FakeRagRetriever()

    result = MultiQuestionRagRetriever(rag_retriever=rag_retriever).retrieve(
        "What is a crusher? And how do I make iron ingot?"
    )

    assert rag_retriever.calls == [
        "What is a crusher?",
        "how do I make iron ingot?",
    ]
    assert result.is_multi_question is True
    assert [item.question for item in result.sub_question_results] == [
        "What is a crusher?",
        "how do I make iron ingot?",
    ]
    assert "[SUB_QUESTION 1]" in result.context_text
    assert "[SUB_QUESTION 2]" in result.context_text
    assert result.metadata == {
        "sub_question_count": 2,
        "max_sub_questions": 3,
        "truncated": False,
        "confidence_counts": {"high": 2, "medium": 0, "low": 0},
    }


def test_multi_question_retriever_keeps_single_question_flow() -> None:
    rag_retriever = FakeRagRetriever()

    result = MultiQuestionRagRetriever(rag_retriever=rag_retriever).retrieve(
        "What is a crusher?"
    )

    assert rag_retriever.calls == ["What is a crusher?"]
    assert result.is_multi_question is False
    assert result.metadata["sub_question_count"] == 1


def _retrieval_result(query: str) -> ManualRagRetrievalResult:
    return ManualRagRetrievalResult(
        query=query,
        results=[
            ManualRagSearchResult(
                doc_id=f"doc:{query}",
                title=query,
                content=f"{query} answer context",
                source_file="equipment.csv",
                source_row_id=f"doc:{query}",
                metadata={"record_type": "equipment"},
                score=0.9,
            )
        ],
        top_score=0.9,
        matched_documents=1,
        context_text=f"context for {query}",
        confidence="high",
        confidence_reason="direct_match_high_score",
        retrieval_metadata={
            "top_score": 0.9,
            "matched_documents": 1,
            "direct_match": True,
        },
    )
