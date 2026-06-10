"""Embedding providers for operator_guide RAG ingestion."""

from __future__ import annotations

import logging
import os
from collections.abc import Mapping
from dataclasses import dataclass
from typing import Literal, Protocol

from openai import OpenAI

EmbeddingProviderName = Literal["none", "openai"]

_DEFAULT_OPENAI_EMBEDDING_MODEL = "text-embedding-3-small"
_DEFAULT_OPENAI_EMBEDDING_DIMENSIONS = 1536
_EMBEDDING_PROVIDERS: set[str] = {"none", "openai"}

logger = logging.getLogger(__name__)


class _OpenAIEmbeddingItem(Protocol):
    embedding: list[float]


class _OpenAIEmbeddingResponse(Protocol):
    data: list[_OpenAIEmbeddingItem]


class _OpenAIEmbeddingsClient(Protocol):
    def create(
        self,
        *,
        model: str,
        input: list[str],
        dimensions: int | None = None,
    ) -> _OpenAIEmbeddingResponse:
        """Create embeddings for input texts."""


class _OpenAIClient(Protocol):
    embeddings: _OpenAIEmbeddingsClient


@dataclass(frozen=True)
class EmbeddingSettings:
    """Embedding provider settings for Manual Q&A RAG ingestion."""

    provider: EmbeddingProviderName
    model: str | None = None
    dimensions: int | None = None
    api_key: str | None = None
    base_url: str | None = None

    @classmethod
    def from_env(cls, env: Mapping[str, str] | None = None) -> EmbeddingSettings:
        source = os.environ if env is None else env
        provider = _provider_from_env(source)
        if provider == "none":
            return cls(provider="none")
        return cls(
            provider="openai",
            model=_string_from_env(source, "FACTORY_EMBEDDING_MODEL")
            or _DEFAULT_OPENAI_EMBEDDING_MODEL,
            dimensions=_int_from_env(
                source,
                "FACTORY_EMBEDDING_DIMENSIONS",
                _DEFAULT_OPENAI_EMBEDDING_DIMENSIONS,
            ),
            api_key=_string_from_env(source, "FACTORY_EMBEDDING_API_KEY")
            or _string_from_env(source, "OPENAI_API_KEY"),
            base_url=_string_from_env(source, "FACTORY_EMBEDDING_BASE_URL"),
        )


@dataclass(frozen=True)
class NoopEmbeddingProvider:
    """Disabled embedding provider."""

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        _ = texts
        return []


@dataclass(frozen=True)
class OpenAIEmbeddingProvider:
    """OpenAI embedding provider using the official SDK."""

    settings: EmbeddingSettings
    client: _OpenAIClient | None = None
    timeout_ms: int = 20000

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        if not texts:
            return []
        if not self.settings.api_key:
            return []
        if not self.settings.model:
            return []

        logger.info("Calling OpenAI embeddings (model: %s)", self.settings.model)
        try:
            client = self.client or _create_openai_client(
                api_key=self.settings.api_key,
                base_url=self.settings.base_url,
                timeout_ms=self.timeout_ms,
            )
            response = client.embeddings.create(
                model=self.settings.model,
                input=texts,
                dimensions=self.settings.dimensions,
            )
        except Exception as exc:
            logger.warning("OpenAI embedding call failed: %s", exc)
            return []

        return [item.embedding for item in response.data]


def create_embedding_provider(
    settings: EmbeddingSettings | None = None,
) -> NoopEmbeddingProvider | OpenAIEmbeddingProvider:
    resolved_settings = settings or EmbeddingSettings.from_env()
    if resolved_settings.provider == "none":
        return NoopEmbeddingProvider()
    return OpenAIEmbeddingProvider(resolved_settings)


def _create_openai_client(
    *,
    api_key: str,
    base_url: str | None,
    timeout_ms: int,
) -> _OpenAIClient:
    timeout_seconds = timeout_ms / 1000
    if base_url:
        return OpenAI(api_key=api_key, base_url=base_url, timeout=timeout_seconds)
    return OpenAI(api_key=api_key, timeout=timeout_seconds)


def _provider_from_env(env: Mapping[str, str]) -> EmbeddingProviderName:
    provider = _string_from_env(env, "FACTORY_EMBEDDING_PROVIDER") or "none"
    if provider not in _EMBEDDING_PROVIDERS:
        raise ValueError(f"Unsupported embedding provider: {provider}")
    return provider  # type: ignore[return-value]


def _string_from_env(env: Mapping[str, str], key: str) -> str | None:
    value = env.get(key)
    if value is None:
        return None
    stripped = value.strip()
    return stripped or None


def _int_from_env(env: Mapping[str, str], key: str, default: int) -> int:
    value = _string_from_env(env, key)
    return default if value is None else int(value)
