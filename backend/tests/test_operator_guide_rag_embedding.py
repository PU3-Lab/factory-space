"""Tests for Manual Q&A RAG embedding providers."""

from __future__ import annotations

import json
import urllib.error
from unittest.mock import MagicMock, patch

import pytest

from agents.operator_guide.rag_embedding import (
    EmbeddingSettings,
    LocalEmbeddingProvider,
    NoopEmbeddingProvider,
    OpenAIEmbeddingProvider,
    create_embedding_provider,
)


class FakeEmbeddingItem:
    def __init__(self, embedding: list[float]) -> None:
        self.embedding = embedding


class FakeEmbeddingResponse:
    def __init__(self, embeddings: list[list[float]]) -> None:
        self.data = [FakeEmbeddingItem(embedding) for embedding in embeddings]


class FakeEmbeddingsClient:
    def __init__(self) -> None:
        self.calls: list[dict[str, object]] = []

    def create(
        self,
        *,
        model: str,
        input: list[str],
        dimensions: int | None = None,
    ) -> FakeEmbeddingResponse:
        self.calls.append(
            {
                "model": model,
                "input": input,
                "dimensions": dimensions,
            },
        )
        return FakeEmbeddingResponse([[1.0, 0.0], [0.0, 1.0]])


class FakeOpenAIClient:
    def __init__(self) -> None:
        self.embeddings = FakeEmbeddingsClient()


def test_embedding_settings_from_env_uses_openai_defaults() -> None:
    settings = EmbeddingSettings.from_env(
        {
            "FACTORY_EMBEDDING_PROVIDER": "openai",
            "OPENAI_API_KEY": "test-key",
        },
    )

    assert settings.provider == "openai"
    assert settings.model == "text-embedding-3-small"
    assert settings.dimensions == 1536
    assert settings.api_key == "test-key"


def test_embedding_settings_prefers_slot_api_key() -> None:
    settings = EmbeddingSettings.from_env(
        {
            "FACTORY_EMBEDDING_PROVIDER": "openai",
            "FACTORY_EMBEDDING_API_KEY": "embedding-key",
            "OPENAI_API_KEY": "openai-key",
        },
    )

    assert settings.api_key == "embedding-key"


def test_embedding_settings_rejects_unsupported_provider() -> None:
    with pytest.raises(ValueError, match="Unsupported embedding provider"):
        EmbeddingSettings.from_env({"FACTORY_EMBEDDING_PROVIDER": "unknown"})


def test_openai_embedding_provider_sends_expected_payload() -> None:
    client = FakeOpenAIClient()
    provider = OpenAIEmbeddingProvider(
        EmbeddingSettings(
            provider="openai",
            model="text-embedding-3-small",
            dimensions=1536,
            api_key="test-key",
        ),
        client=client,
    )

    embeddings = provider.embed_texts(["제련기", "컨베이어"])

    assert embeddings == [[1.0, 0.0], [0.0, 1.0]]
    assert client.embeddings.calls == [
        {
            "model": "text-embedding-3-small",
            "input": ["제련기", "컨베이어"],
            "dimensions": 1536,
        },
    ]


def test_openai_embedding_provider_returns_empty_without_api_key() -> None:
    provider = OpenAIEmbeddingProvider(
        EmbeddingSettings(provider="openai", model="text-embedding-3-small"),
    )

    assert provider.embed_texts(["제련기"]) == []


def test_create_embedding_provider_returns_noop_for_none_provider() -> None:
    provider = create_embedding_provider(EmbeddingSettings(provider="none"))

    assert isinstance(provider, NoopEmbeddingProvider)
    assert provider.embed_texts(["제련기"]) == []


def test_embedding_settings_from_env_uses_local_defaults() -> None:
    settings = EmbeddingSettings.from_env(
        {
            "FACTORY_EMBEDDING_PROVIDER": "local",
        },
    )

    assert settings.provider == "local"
    assert settings.model == "nomic-embed-text"
    assert settings.dimensions == 768
    assert settings.api_key == "noop"
    assert settings.base_url == "http://localhost:11434"


def test_local_embedding_provider_openai_compatible_mode() -> None:
    client = FakeOpenAIClient()
    provider = LocalEmbeddingProvider(
        EmbeddingSettings(
            provider="local",
            model="nomic-embed-text",
            dimensions=768,
            api_key="noop",
            base_url="http://localhost:11434/v1",
        ),
        client=client,
    )

    embeddings = provider.embed_texts(["제련기", "컨베이어"])

    assert embeddings == [[1.0, 0.0], [0.0, 1.0]]
    assert client.embeddings.calls == [
        {
            "model": "nomic-embed-text",
            "input": ["제련기", "컨베이어"],
            "dimensions": 768,
        },
    ]


def test_local_embedding_provider_ollama_native_mode() -> None:
    provider = LocalEmbeddingProvider(
        EmbeddingSettings(
            provider="local",
            model="nomic-embed-text",
            dimensions=768,
            api_key="noop",
            base_url="http://localhost:11434",
        ),
    )

    mock_response = MagicMock()
    mock_response.__enter__.return_value = mock_response
    mock_response.read.side_effect = [
        b'{"embedding": [0.5, 0.5]}',
        b'{"embedding": [0.1, 0.9]}',
    ]

    with patch("urllib.request.urlopen", return_value=mock_response) as mock_urlopen:
        embeddings = provider.embed_texts(["제련기", "컨베이어"])

        assert embeddings == [[0.5, 0.5], [0.1, 0.9]]
        assert mock_urlopen.call_count == 2
        req1 = mock_urlopen.call_args_list[0][0][0]
        assert req1.full_url == "http://localhost:11434/api/embeddings"
        assert json.loads(req1.data.decode("utf-8")) == {
            "model": "nomic-embed-text",
            "prompt": "제련기",
        }


def test_local_embedding_provider_ollama_native_failure() -> None:
    provider = LocalEmbeddingProvider(
        EmbeddingSettings(
            provider="local",
            model="nomic-embed-text",
            dimensions=768,
            api_key="noop",
            base_url="http://localhost:11434",
        ),
    )

    with patch(
        "urllib.request.urlopen",
        side_effect=urllib.error.URLError("Connection refused"),
    ):
        embeddings = provider.embed_texts(["제련기"])
        assert embeddings == []


def test_create_embedding_provider_returns_local_provider() -> None:
    provider = create_embedding_provider(
        EmbeddingSettings(
            provider="local",
            model="nomic-embed-text",
            dimensions=768,
            base_url="http://localhost:11434",
        )
    )

    assert isinstance(provider, LocalEmbeddingProvider)
