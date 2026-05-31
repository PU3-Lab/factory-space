from __future__ import annotations

from llm.adapter import (
    GoogleGenAiLlmAdapter,
    LocalLlmAdapter,
    NoopLlmAdapter,
    OpenAiLlmAdapter,
    create_llm_adapter,
)
from llm.settings import LlmModelSlot


class FakeGoogleResponse:
    def __init__(self, text: object) -> None:
        self.text = text


class FakeGoogleModels:
    def __init__(
        self,
        response: FakeGoogleResponse | None = None,
        error: Exception | None = None,
    ) -> None:
        self.response = response or FakeGoogleResponse('{"summary":"ok"}')
        self.error = error
        self.calls: list[dict[str, object]] = []

    def generate_content(
        self,
        *,
        model: str,
        contents: str,
        config: object,
    ) -> FakeGoogleResponse:
        self.calls.append(
            {
                "model": model,
                "contents": contents,
                "config": config,
            }
        )
        if self.error is not None:
            raise self.error
        return self.response


class FakeGoogleClient:
    def __init__(self, models: FakeGoogleModels) -> None:
        self.models = models


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


def test_google_llm_adapter_returns_response_text() -> None:
    models = FakeGoogleModels(FakeGoogleResponse('  {"summary":"ok"}  '))
    adapter = GoogleGenAiLlmAdapter(
        LlmModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="key",
        ),
        client=FakeGoogleClient(models),
        timeout_ms=1234,
        max_output_tokens=64,
        temperature=0.1,
    )

    result = adapter.invoke("prompt")

    assert result == '  {"summary":"ok"}  '
    assert models.calls[0]["model"] == "gemini-2.5-flash"
    assert models.calls[0]["contents"] == "prompt"
    config = models.calls[0]["config"]
    assert getattr(config, "response_mime_type") == "application/json"
    assert getattr(config, "max_output_tokens") == 64
    assert getattr(config, "temperature") == 0.1
    assert getattr(config.http_options, "timeout") == 1234


def test_google_llm_adapter_returns_none_for_empty_response() -> None:
    models = FakeGoogleModels(FakeGoogleResponse(""))
    adapter = GoogleGenAiLlmAdapter(
        LlmModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="key",
        ),
        client=FakeGoogleClient(models),
    )

    assert adapter.invoke("prompt") is None


def test_google_llm_adapter_returns_none_for_non_string_response_text() -> None:
    models = FakeGoogleModels(FakeGoogleResponse({"summary": "ok"}))
    adapter = GoogleGenAiLlmAdapter(
        LlmModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="key",
        ),
        client=FakeGoogleClient(models),
    )

    assert adapter.invoke("prompt") is None


def test_google_llm_adapter_returns_none_without_api_key() -> None:
    adapter = GoogleGenAiLlmAdapter(
        LlmModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="",
        )
    )

    assert adapter.invoke("prompt") is None


def test_google_llm_adapter_returns_none_for_provider_error() -> None:
    models = FakeGoogleModels(error=RuntimeError("provider failed"))
    adapter = GoogleGenAiLlmAdapter(
        LlmModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="key",
        ),
        client=FakeGoogleClient(models),
    )

    assert adapter.invoke("prompt") is None
