from __future__ import annotations

from llm.adapter import (
    GoogleGenAiLlmAdapter,
    LocalLlmAdapter,
    NoopLlmAdapter,
    OpenAiLlmAdapter,
    create_llm_adapter,
)
from llm.settings import LlmModelSlot


def test_noop_llm_adapter_returns_none() -> None:
    adapter = NoopLlmAdapter()

    assert adapter.invoke("prompt") is None


def test_create_llm_adapter_returns_noop_for_none_slot() -> None:
    slot = LlmModelSlot(name="default", provider="none")

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, NoopLlmAdapter)


def test_create_llm_adapter_returns_google_adapter_for_google_slot() -> None:
    slot = LlmModelSlot(
        name="default",
        provider="google",
        model="gemini-2.5-flash",
        api_key="key",
    )

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, GoogleGenAiLlmAdapter)


def test_create_llm_adapter_returns_openai_adapter_for_openai_slot() -> None:
    slot = LlmModelSlot(
        name="fallback1",
        provider="openai",
        model="gpt-5.5",
        api_key="key",
    )

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, OpenAiLlmAdapter)


def test_create_llm_adapter_returns_local_adapter_for_local_slot() -> None:
    slot = LlmModelSlot(
        name="fallback2",
        provider="local",
        model="llama3.1:8b",
        base_url="http://localhost:11434/v1",
    )

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, LocalLlmAdapter)
