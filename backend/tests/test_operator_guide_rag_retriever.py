"""Tests for operator_guide RAG retrieval runtime."""

from __future__ import annotations

from agents.operator_guide.rag_retriever import (
    ManualRagRetriever,
    ManualRagRetrieverSettings,
    ManualRagSearchResult,
)


class FakeEmbeddingProvider:
    def __init__(self, embedding: list[float] | None = None) -> None:
        self.embedding = embedding or [0.1, 0.2]
        self.calls: list[list[str]] = []

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        self.calls.append(texts)
        return [self.embedding for _ in texts]


class EmptyEmbeddingProvider:
    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        _ = texts
        return []


class FakeSearchStore:
    def __init__(self, results: list[ManualRagSearchResult]) -> None:
        self.results = results
        self.calls: list[dict[str, object]] = []

    def search_similar(
        self,
        query_embedding: list[float],
        *,
        top_k: int,
    ) -> list[ManualRagSearchResult]:
        self.calls.append({"query_embedding": query_embedding, "top_k": top_k})
        return self.results[:top_k]


def test_retriever_embeds_query_and_searches_top_k_documents() -> None:
    provider = FakeEmbeddingProvider([0.9, 0.1])
    store = FakeSearchStore(
        [
            _result("equipment:smelter", "제련기", 0.91),
            _result("recipe:iron_ingot", "철괴 제작", 0.82),
        ],
    )

    result = ManualRagRetriever(
        embedding_provider=provider,
        search_store=store,
        settings=ManualRagRetrieverSettings(top_k=1, max_context_chars=1000),
    ).retrieve("철괴는 어떻게 만들어?")

    assert provider.calls == [["철괴는 어떻게 만들어?"]]
    assert store.calls == [{"query_embedding": [0.9, 0.1], "top_k": 1}]
    assert result.query == "철괴는 어떻게 만들어?"
    assert result.matched_documents == 1
    assert result.top_score == 0.91
    assert [item.doc_id for item in result.results] == ["equipment:smelter"]
    assert "제련기" in result.context_text


def test_retriever_returns_empty_result_when_embedding_is_unavailable() -> None:
    store = FakeSearchStore([_result("equipment:smelter", "제련기", 0.91)])

    result = ManualRagRetriever(
        embedding_provider=EmptyEmbeddingProvider(),
        search_store=store,
    ).retrieve("철괴는 어떻게 만들어?")

    assert store.calls == []
    assert result.results == []
    assert result.matched_documents == 0
    assert result.top_score is None
    assert result.context_text == ""


def test_retriever_limits_context_text_length() -> None:
    long_content = "가" * 200
    store = FakeSearchStore(
        [
            _result("doc:long", "긴 문서", 0.7, content=long_content),
        ],
    )

    result = ManualRagRetriever(
        embedding_provider=FakeEmbeddingProvider(),
        search_store=store,
        settings=ManualRagRetrieverSettings(top_k=1, max_context_chars=80),
    ).retrieve("긴 문서 찾아줘")

    assert len(result.context_text) <= 80
    assert result.context_text.endswith("...")


def test_retriever_keeps_source_metadata_in_results() -> None:
    store = FakeSearchStore([_result("resource:iron_ore", "철광석", 0.88)])

    result = ManualRagRetriever(
        embedding_provider=FakeEmbeddingProvider(),
        search_store=store,
    ).retrieve("철광석은 어디에 써?")

    source = result.results[0]
    assert source.source_file == "equipment.csv"
    assert source.source_row_id == "resource:iron_ore"
    assert source.metadata == {"record_type": "equipment"}


def _result(
    doc_id: str,
    title: str,
    score: float,
    *,
    content: str | None = None,
) -> ManualRagSearchResult:
    return ManualRagSearchResult(
        doc_id=doc_id,
        title=title,
        content=content or f"{title} 설명입니다.",
        source_file="equipment.csv",
        source_row_id=doc_id,
        metadata={"record_type": "equipment"},
        score=score,
    )
