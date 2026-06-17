"""operator_guide RAG 문서를 벡터로 바꾸는 embedding provider 모듈.

초보자용 설명:
    LLM 검색에서 "비슷한 문서"를 찾으려면 텍스트를 숫자 배열(vector)로 바꿔야 한다.
    이 파일은 OpenAI embedding API 또는 테스트용 no-op provider를 같은 인터페이스로 다룬다.
"""

from __future__ import annotations

import json
import logging
import os
import urllib.request
from collections.abc import Mapping
from dataclasses import dataclass
from typing import Literal, Protocol

from openai import OpenAI

EmbeddingProviderName = Literal["none", "openai", "local"]

_DEFAULT_OPENAI_EMBEDDING_MODEL = "text-embedding-3-small"
_DEFAULT_OPENAI_EMBEDDING_DIMENSIONS = 1536
_EMBEDDING_PROVIDERS: set[str] = {"none", "openai", "local"}

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
        """텍스트 목록을 OpenAI embedding 응답으로 변환한다."""


class _OpenAIClient(Protocol):
    embeddings: _OpenAIEmbeddingsClient


@dataclass(frozen=True)
class EmbeddingSettings:
    """embedding 실행에 필요한 설정 묶음.

    env 파일에서 provider, model, dimensions, api key를 읽어 온다.
    """

    provider: EmbeddingProviderName
    model: str | None = None
    dimensions: int | None = None
    api_key: str | None = None
    base_url: str | None = None

    @classmethod
    def from_env(cls, env: Mapping[str, str] | None = None) -> EmbeddingSettings:
        """환경변수에서 embedding 설정을 만든다.

        초보자용 설명:
            환경변수(`FACTORY_EMBEDDING_PROVIDER` 등)를 읽어서 RAG 임베딩 실행에 필요한 설정을 구축합니다.
            `provider`가 `"local"`인 경우 로컬 LLM 서버에 호환되는 기본값(nomic-embed-text, 768차원 등)을 사용합니다.
        """

        source = os.environ if env is None else env
        provider = _provider_from_env(source)
        if provider == "none":
            return cls(provider="none")

        if provider == "local":
            return cls(
                provider="local",
                model=_string_from_env(source, "FACTORY_EMBEDDING_MODEL")
                or "nomic-embed-text",
                dimensions=_int_from_env(
                    source,
                    "FACTORY_EMBEDDING_DIMENSIONS",
                    768,
                ),
                api_key=_string_from_env(source, "FACTORY_EMBEDDING_API_KEY")
                or _string_from_env(source, "OPENAI_API_KEY")
                or "noop",
                base_url=_string_from_env(source, "FACTORY_EMBEDDING_BASE_URL")
                or "http://localhost:11434",
            )

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
    """embedding을 실제로 만들지 않는 provider.

    테스트나 비활성화 환경에서 외부 API 호출을 막기 위해 사용한다.
    """

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        _ = texts
        return []


@dataclass(frozen=True)
class OpenAIEmbeddingProvider:
    """OpenAI SDK로 텍스트 embedding을 생성하는 provider."""

    settings: EmbeddingSettings
    client: _OpenAIClient | None = None
    timeout_ms: int = 20000

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """여러 문장을 한 번에 embedding vector 목록으로 바꾼다."""

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


@dataclass(frozen=True)
class LocalEmbeddingProvider:
    """로컬 LLM 환경(Ollama, LM Studio 등)에서 텍스트 embedding을 생성하는 provider.

    초보자용 설명:
        이 프로바이더는 로컬에서 실행되는 AI 모델 서버에 임베딩을 요청합니다.
        서버 주소(base_url)가 '/v1'로 끝나면 OpenAI와 똑같은 방식으로 요청을 보내고(OpenAI 호환 모드),
        그렇지 않으면 Ollama 자체 API인 '/api/embeddings' 주소로 요청을 보냅니다(Ollama 네이티브 모드).
    """

    settings: EmbeddingSettings
    client: _OpenAIClient | None = None
    timeout_ms: int = 20000

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """여러 문장을 로컬 임베딩 vector 목록으로 변환합니다.

        초보자용 설명:
            입력받은 텍스트 리스트를 순서대로 임베딩 벡터로 바꿉니다.
            네트워크 연결이 끊기거나 모델이 없을 때는 에러가 나서 프로그램이 멈추지 않도록,
            경고 로그만 남기고 빈 리스트 `[]`를 반환합니다.
        """
        if not texts:
            return []

        base_url = self.settings.base_url or ""
        normalized_url = base_url.strip().rstrip("/")

        if normalized_url.endswith("/v1"):
            # OpenAI 호환 모드
            logger.info("Calling local OpenAI-compatible embeddings (model: %s)", self.settings.model)
            try:
                client = self.client or _create_openai_client(
                    api_key=self.settings.api_key or "noop",
                    base_url=self.settings.base_url,
                    timeout_ms=self.timeout_ms,
                )
                response = client.embeddings.create(
                    model=self.settings.model or "nomic-embed-text",
                    input=texts,
                    dimensions=self.settings.dimensions,
                )
            except Exception as exc:
                logger.warning("Local OpenAI-compatible embedding call failed: %s", exc)
                return []
            return [item.embedding for item in response.data]

        else:
            # Ollama 네이티브 모드
            logger.info("Calling local Ollama native embeddings (model: %s)", self.settings.model)
            embeddings = []
            url = f"{normalized_url}/api/embeddings"
            for text in texts:
                try:
                    payload = {
                        "model": self.settings.model or "nomic-embed-text",
                        "prompt": text,
                    }
                    data = json.dumps(payload).encode("utf-8")
                    req = urllib.request.Request(
                        url,
                        data=data,
                        headers={"Content-Type": "application/json"},
                    )
                    with urllib.request.urlopen(req, timeout=self.timeout_ms / 1000) as response:
                        res_data = json.loads(response.read().decode("utf-8"))
                        embeddings.append(res_data["embedding"])
                except Exception as exc:
                    logger.warning("Local Ollama native embedding call failed: %s", exc)
                    return []
            return embeddings


def create_embedding_provider(
    settings: EmbeddingSettings | None = None,
) -> NoopEmbeddingProvider | OpenAIEmbeddingProvider | LocalEmbeddingProvider:
    """설정에 맞는 embedding provider를 만들어 반환한다.

    초보자용 설명:
        설정(settings)에서 지정된 프로바이더 종류("none", "openai", "local")에 따라
        각각 NoopEmbeddingProvider, OpenAIEmbeddingProvider, LocalEmbeddingProvider 중
        적절한 객체를 생성해서 반환해 줍니다.
    """

    resolved_settings = settings or EmbeddingSettings.from_env()
    if resolved_settings.provider == "none":
        return NoopEmbeddingProvider()
    if resolved_settings.provider == "local":
        return LocalEmbeddingProvider(resolved_settings)
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
