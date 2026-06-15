"""operator_guide가 질문과 비슷한 RAG 문서를 찾는 런타임 모듈.

초보자용 설명:
    ingestion 단계에서는 CSV 문서를 embedding해서 DB에 넣는다.
    이 파일은 반대로 플레이어 질문을 embedding한 뒤, DB에서 비슷한 문서를 찾아
    LLM prompt에 넣기 좋은 context 문자열로 정리한다.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


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
        return ManualRagRetrievalResult(
            query=query,
            results=results,
            top_score=results[0].score if results else None,
            matched_documents=len(results),
            context_text=_build_context_text(
                results,
                max_chars=self._settings.max_context_chars,
            ),
        )

    def _embed_query(self, query: str) -> list[float] | None:
        embeddings = self._embedding_provider.embed_texts([query])
        if not embeddings:
            return None
        return embeddings[0]


def _empty_result(query: str) -> ManualRagRetrievalResult:
    return ManualRagRetrievalResult(
        query=query,
        results=[],
        top_score=None,
        matched_documents=0,
        context_text="",
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
