"""Tests for the operator_guide RAG debug router."""

from __future__ import annotations

import os
from dataclasses import dataclass
from unittest.mock import patch

from fastapi.testclient import TestClient

from agents.operator_guide.service import ManualQAService
from app import create_app


@dataclass
class MockSearchResult:
    doc_id: str
    title: str
    content: str
    source_file: str
    source_row_id: str
    score: float


@dataclass
class MockRetrievalResult:
    confidence: str
    results: list[MockSearchResult]


@dataclass
class MockSubQuestionResult:
    index: int
    question: str
    retrieval: MockRetrievalResult


@dataclass
class MockRagResult:
    is_multi_question: bool
    sub_question_results: list[MockSubQuestionResult]
    metadata: dict[str, object]


class MockRagRuntime:
    def retrieve(self, question: str) -> MockRagResult:
        return MockRagResult(
            is_multi_question=True,
            sub_question_results=[
                MockSubQuestionResult(
                    index=1,
                    question="What is a crusher?",
                    retrieval=MockRetrievalResult(
                        confidence="high",
                        results=[
                            MockSearchResult(
                                doc_id="equipment.grinder",
                                title="Grinder Manual",
                                content="Grinds resources.",
                                source_file="equipment.csv",
                                source_row_id="1",
                                score=0.9,
                            )
                        ],
                    ),
                )
            ],
            metadata={"confidence_counts": {"high": 1, "medium": 0, "low": 0}},
        )


def test_debug_search_forbidden_when_debug_flag_disabled() -> None:
    with patch.dict(
        os.environ,
        {"FACTORY_RAG_DEBUG_ENABLED": "false", "FACTORY_RAG_RUNTIME_MOCK": "true"},
    ):
        with TestClient(create_app()) as client:
            response = client.post(
                "/api/v1/debug/manual-rag/search",
                json={"question": "What is a crusher?"},
            )
            assert response.status_code == 403
            assert "disabled" in response.json()["detail"]


def test_debug_search_not_found_when_rag_runtime_missing() -> None:
    with patch.dict(
        os.environ,
        {"FACTORY_RAG_DEBUG_ENABLED": "true", "FACTORY_RAG_RUNTIME_MOCK": "true"},
    ):
        ManualQAService.set_global_rag_runtime(None)
        with TestClient(create_app()) as client:
            response = client.post(
                "/api/v1/debug/manual-rag/search",
                json={"question": "What is a crusher?"},
            )
            assert response.status_code == 404
            assert "RAG runtime is not active" in response.json()["detail"]


def test_debug_search_returns_similar_documents_when_runtime_active() -> None:
    with patch.dict(
        os.environ,
        {"FACTORY_RAG_DEBUG_ENABLED": "true", "FACTORY_RAG_RUNTIME_MOCK": "true"},
    ):
        mock_runtime = MockRagRuntime()
        ManualQAService.set_global_rag_runtime(mock_runtime)
        try:
            with TestClient(create_app()) as client:
                response = client.post(
                    "/api/v1/debug/manual-rag/search",
                    json={"question": "What is a crusher?", "top_k": 1},
                )
                assert response.status_code == 200
                body = response.json()
                assert body["query"] == "What is a crusher?"
                assert body["is_multi_question"] is True
                assert body["confidence"] == "high"
                assert len(body["sub_questions"]) == 1
                assert body["sub_questions"][0]["question"] == "What is a crusher?"
                assert len(body["sub_questions"][0]["results"]) == 1
                assert (
                    body["sub_questions"][0]["results"][0]["doc_id"]
                    == "equipment.grinder"
                )
        finally:
            ManualQAService.set_global_rag_runtime(None)
