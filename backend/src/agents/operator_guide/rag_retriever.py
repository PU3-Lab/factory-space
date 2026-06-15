"""operator_guide가 질문과 비슷한 RAG 문서를 찾는 런타임 모듈.

초보자용 설명:
    ingestion 단계에서는 CSV 문서를 embedding해서 DB에 넣는다.
    이 파일은 반대로 플레이어 질문을 embedding한 뒤, DB에서 비슷한 문서를 찾아
    LLM prompt에 넣기 좋은 context 문자열로 정리한다.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Literal, Protocol

RetrievalConfidence = Literal["high", "medium", "low"]


class QueryEmbeddingProvider(Protocol):
    """플레이어 질문을 embedding vector로 바꾸는 객체의 최소 약속."""

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """입력 텍스트마다 embedding vector 하나를 반환한다."""


@dataclass(frozen=True)
class ManualRagSearchResult:
    """pgvector 검색으로 찾은 RAG 문서 한 개."""

    doc_id: str
    title: str
    content: str
    source_file: str
    source_row_id: str
    metadata: dict[str, str]
    score: float


class ManualRagSearchStore(Protocol):
    """RAG retriever가 기대하는 검색 저장소 인터페이스."""

    def search_similar(
        self,
        query_embedding: list[float],
        *,
        top_k: int,
    ) -> list[ManualRagSearchResult]:
        """질문 embedding과 가까운 active RAG 문서를 반환한다."""


@dataclass(frozen=True)
class ManualRagRetrieverSettings:
    """RAG 검색 런타임 설정."""

    top_k: int = 5
    max_context_chars: int = 6000


@dataclass(frozen=True)
class ManualRagRetrievalResult:
    """한 번의 RAG 검색 결과 묶음."""

    query: str
    results: list[ManualRagSearchResult]
    top_score: float | None
    matched_documents: int
    context_text: str
    # confidence는 LLM 감상이 아니라 검색 점수와 매칭 신호를 기준으로 backend가 계산한다.
    confidence: RetrievalConfidence
    confidence_reason: str
    retrieval_metadata: dict[str, object]


class ManualRagRetriever:
    """질문 embedding, pgvector 검색, prompt context 생성을 조율한다."""

    def __init__(
        self,
        *,
        embedding_provider: QueryEmbeddingProvider,
        search_store: ManualRagSearchStore,
        settings: ManualRagRetrieverSettings | None = None,
    ) -> None:
        self._embedding_provider = embedding_provider
        self._search_store = search_store
        self._settings = settings or ManualRagRetrieverSettings()

    def retrieve(self, query: str) -> ManualRagRetrievalResult:
        """플레이어 질문과 관련된 RAG 문서를 검색한다."""

        query_embedding = self._embed_query(query)
        if query_embedding is None:
            return _empty_result(query)

        results = self._search_store.search_similar(
            query_embedding,
            top_k=self._settings.top_k,
        )
        confidence_result = _calculate_confidence(query, results)
        return ManualRagRetrievalResult(
            query=query,
            results=results,
            top_score=results[0].score if results else None,
            matched_documents=len(results),
            context_text=_build_context_text(
                results,
                max_chars=self._settings.max_context_chars,
            ),
            confidence=confidence_result.confidence,
            confidence_reason=confidence_result.reason,
            retrieval_metadata=confidence_result.metadata,
        )

    def _embed_query(self, query: str) -> list[float] | None:
        embeddings = self._embedding_provider.embed_texts([query])
        if not embeddings:
            return None
        return embeddings[0]


def _empty_result(query: str) -> ManualRagRetrievalResult:
    confidence_result = _calculate_confidence(query, [])
    return ManualRagRetrievalResult(
        query=query,
        results=[],
        top_score=None,
        matched_documents=0,
        context_text="",
        confidence=confidence_result.confidence,
        confidence_reason=confidence_result.reason,
        retrieval_metadata=confidence_result.metadata,
    )


@dataclass(frozen=True)
class _ConfidenceResult:
    confidence: RetrievalConfidence
    reason: str
    metadata: dict[str, object]


def _calculate_confidence(
    query: str,
    results: list[ManualRagSearchResult],
) -> _ConfidenceResult:
    top_score = results[0].score if results else None
    direct_match = _has_direct_match(query, results)
    metadata = {
        "top_score": top_score,
        "matched_documents": len(results),
        "direct_match": direct_match,
    }

    if top_score is None:
        return _ConfidenceResult(
            confidence="low",
            reason="no_retrieval_results",
            metadata=metadata,
        )
    if top_score >= 0.85 and direct_match:
        return _ConfidenceResult(
            confidence="high",
            reason="direct_match_high_score",
            metadata=metadata,
        )
    if top_score >= 0.65:
        return _ConfidenceResult(
            confidence="medium",
            reason="related_documents_medium_score",
            metadata=metadata,
        )
    return _ConfidenceResult(
        confidence="low",
        reason="weak_retrieval_score",
        metadata=metadata,
    )


def _has_direct_match(query: str, results: list[ManualRagSearchResult]) -> bool:
    normalized_query = _normalize_match_text(query)
    return any(
        token
        for result in results
        for token in (
            _normalize_match_text(result.title),
            _normalize_match_text(result.doc_id),
            _normalize_match_text(result.source_row_id),
        )
        if token and token in normalized_query
    )


def _normalize_match_text(value: str) -> str:
    return (
        value.lower()
        .replace("_", " ")
        .replace("-", " ")
        .replace(":", " ")
        .strip()
    )


def _build_context_text(
    results: list[ManualRagSearchResult],
    *,
    max_chars: int,
) -> str:
    parts = [_format_source(index, result) for index, result in enumerate(results, start=1)]
    context = "\n\n".join(parts)
    if len(context) <= max_chars:
        return context
    if max_chars <= 3:
        return "." * max_chars
    return f"{context[: max_chars - 3]}..."


def _format_source(index: int, result: ManualRagSearchResult) -> str:
    return "\n".join(
        [
            f"[SOURCE {index}]",
            f"doc_id: {result.doc_id}",
            f"title: {result.title}",
            f"source_file: {result.source_file}",
            f"source_row_id: {result.source_row_id}",
            f"score: {result.score:.4f}",
            "content:",
            result.content,
        ],
    )
