"""LLM provider adapters."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from llm.settings import LlmModelSlot


class LlmAdapter(Protocol):
    """Common contract for raw LLM text generation."""

    def invoke(self, prompt: str) -> str | None:
        """Return raw model output, or None when unavailable."""


class _GoogleModelsClient(Protocol):
    def generate_content(
        self,
        *,
        model: str,
        contents: str,
        config: object,
    ) -> object:
        """Generate content with the Google Gen AI SDK."""


class _GoogleClient(Protocol):
    models: _GoogleModelsClient


@dataclass(frozen=True)
class NoopLlmAdapter:
    """Disabled LLM adapter."""

    def invoke(self, prompt: str) -> str | None:
        """Return no output."""

        return None


@dataclass(frozen=True)
class GoogleGenAiLlmAdapter:
    """Google Gen AI adapter."""

    slot: LlmModelSlot
    client: _GoogleClient | None = None
    timeout_ms: int = 20000
    max_output_tokens: int = 2048
    temperature: float = 0.2

    def invoke(self, prompt: str) -> str | None:
        """Return raw generated text from Google Gen AI."""

        if not self.slot.model:
            return None
        try:
            client = self.client or _create_google_client(self.slot.api_key)
            if client is None:
                return None
            response = client.models.generate_content(
                model=self.slot.model,
                contents=prompt,
                config=_google_generate_config(
                    timeout_ms=self.timeout_ms,
                    max_output_tokens=self.max_output_tokens,
                    temperature=self.temperature,
                ),
            )
        except Exception:
            return None

        text = getattr(response, "text", None)
        if not isinstance(text, str):
            return None
        if not text.strip():
            return None
        return text


@dataclass(frozen=True)
class OpenAiLlmAdapter:
    """OpenAI-compatible adapter contract placeholder."""

    slot: LlmModelSlot

    def invoke(self, prompt: str) -> str | None:
        """Return no output until provider calls are implemented."""

        return None


@dataclass(frozen=True)
class LocalLlmAdapter:
    """Local OpenAI-compatible adapter contract placeholder."""

    slot: LlmModelSlot

    def invoke(self, prompt: str) -> str | None:
        """Return no output until provider calls are implemented."""

        return None


def create_llm_adapter(slot: LlmModelSlot) -> LlmAdapter:
    """Create an adapter for one configured LLM slot."""

    if slot.provider == "none":
        return NoopLlmAdapter()
    if slot.provider == "google":
        return GoogleGenAiLlmAdapter(slot)
    if slot.provider == "openai":
        return OpenAiLlmAdapter(slot)
    return LocalLlmAdapter(slot)


def _create_google_client(api_key: str | None) -> _GoogleClient | None:
    if not api_key:
        return None
    from google import genai

    return genai.Client(api_key=api_key)


def _google_generate_config(
    *,
    timeout_ms: int,
    max_output_tokens: int,
    temperature: float,
) -> object:
    from google.genai import types

    return types.GenerateContentConfig(
        response_mime_type="application/json",
        max_output_tokens=max_output_tokens,
        temperature=temperature,
        http_options=types.HttpOptions(timeout=timeout_ms),
    )
