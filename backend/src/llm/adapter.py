"""LLM provider adapters."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from llm.settings import LlmModelSlot


class LlmAdapter(Protocol):
    """Common contract for raw LLM text generation."""

    def invoke(self, prompt: str) -> str | None:
        """Return raw model output, or None when unavailable."""


@dataclass(frozen=True)
class NoopLlmAdapter:
    """Disabled LLM adapter."""

    def invoke(self, prompt: str) -> str | None:
        """Return no output."""

        return None


@dataclass(frozen=True)
class GoogleGenAiLlmAdapter:
    """Google Gen AI adapter contract placeholder."""

    slot: LlmModelSlot

    def invoke(self, prompt: str) -> str | None:
        """Return no output until provider calls are implemented."""

        return None


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
