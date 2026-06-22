from __future__ import annotations

from typing import Any

import pytest

import llm.adapter as adapter_module
from llm.adapter import (
    GoogleGenAiLLMAdapter,
    LocalLLMAdapter,
    NoopLLMAdapter,
    OpenAILLMAdapter,
    create_llm_adapter,
)
from llm.settings import LLMModelSlot


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


class FakeOpenAiResponse:
    def __init__(self, output_text: object) -> None:
        self.output_text = output_text


class FakeOpenAiResponses:
    def __init__(
        self,
        response: FakeOpenAiResponse | None = None,
        error: Exception | None = None,
    ) -> None:
        self.response = response or FakeOpenAiResponse('{"summary":"ok"}')
        self.error = error
        self.calls: list[dict[str, Any]] = []

    def create(
        self,
        *,
        model: str,
        input: list[dict[str, str]],
        max_output_tokens: int,
        temperature: float,
    ) -> FakeOpenAiResponse:
        self.calls.append(
            {
                "model": model,
                "input": input,
                "max_output_tokens": max_output_tokens,
                "temperature": temperature,
            }
        )
        if self.error is not None:
            raise self.error
        return self.response


class FakeOpenAiResponsesClient:
    def __init__(self, responses: FakeOpenAiResponses) -> None:
        self.responses = responses


class FakeHttpResponse:
    def __init__(self, status_code: int, body: dict[str, Any]) -> None:
        self.status_code = status_code
        self.body = body


class FakeOpenAiHttpClient:
    def __init__(
        self,
        response: FakeHttpResponse | None = None,
        error: Exception | None = None,
    ) -> None:
        self.response = response or FakeHttpResponse(
            200,
            {
                "choices": [
                    {"message": {"content": '{"summary":"ok"}'}},
                ],
            },
        )
        self.error = error
        self.calls: list[dict[str, Any]] = []

    def post(
        self,
        *,
        url: str,
        headers: dict[str, str],
        json_body: dict[str, Any],
        timeout_ms: int,
    ) -> FakeHttpResponse:
        self.calls.append(
            {
                "url": url,
                "headers": headers,
                "json_body": json_body,
                "timeout_ms": timeout_ms,
            }
        )
        if self.error is not None:
            raise self.error
        return self.response


def test_noop_llm_adapter_returns_none() -> None:
    adapter = NoopLLMAdapter()

    assert adapter.invoke("prompt") is None


def test_create_llm_adapter_returns_noop_for_none_slot() -> None:
    slot = LLMModelSlot(name="default", provider="none")

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, NoopLLMAdapter)


def test_create_llm_adapter_returns_google_adapter_for_google_slot() -> None:
    slot = LLMModelSlot(
        name="default",
        provider="google",
        model="gemini-2.5-flash",
        api_key="key",
    )

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, GoogleGenAiLLMAdapter)


def test_create_llm_adapter_returns_openai_adapter_for_openai_slot() -> None:
    slot = LLMModelSlot(
        name="fallback1",
        provider="openai",
        model="gpt-5.5",
        api_key="key",
    )

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, OpenAILLMAdapter)


def test_create_llm_adapter_returns_local_adapter_for_local_slot() -> None:
    slot = LLMModelSlot(
        name="fallback2",
        provider="local",
        model="llama3.1:8b",
        base_url="http://localhost:11434/v1",
    )

    adapter = create_llm_adapter(slot)

    assert isinstance(adapter, LocalLLMAdapter)


def test_google_llm_adapter_returns_response_text() -> None:
    models = FakeGoogleModels(FakeGoogleResponse('  {"summary":"ok"}  '))
    adapter = GoogleGenAiLLMAdapter(
        LLMModelSlot(
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
    assert getattr(config, "response_mime_type") == "text/plain"
    assert getattr(config, "max_output_tokens") == 64
    assert getattr(config, "temperature") == 0.1
    assert getattr(config.http_options, "timeout") == 1234


def test_google_llm_adapter_returns_none_for_empty_response() -> None:
    models = FakeGoogleModels(FakeGoogleResponse(""))
    adapter = GoogleGenAiLLMAdapter(
        LLMModelSlot(
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
    adapter = GoogleGenAiLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="key",
        ),
        client=FakeGoogleClient(models),
    )

    assert adapter.invoke("prompt") is None


def test_google_llm_adapter_returns_none_without_api_key() -> None:
    adapter = GoogleGenAiLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="",
        )
    )

    assert adapter.invoke("prompt") is None


def test_google_llm_adapter_returns_none_when_client_creation_fails(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def raise_client_error(api_key: str | None) -> object:
        raise RuntimeError("client init failed")

    monkeypatch.setattr(adapter_module, "_create_google_client", raise_client_error)
    adapter = GoogleGenAiLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="key",
        )
    )

    assert adapter.invoke("prompt") is None


def test_google_llm_adapter_returns_none_for_provider_error() -> None:
    models = FakeGoogleModels(error=RuntimeError("provider failed"))
    adapter = GoogleGenAiLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="google",
            model="gemini-2.5-flash",
            api_key="key",
        ),
        client=FakeGoogleClient(models),
    )

    assert adapter.invoke("prompt") is None


def test_openai_llm_adapter_uses_responses_api() -> None:
    responses = FakeOpenAiResponses()
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="default",
            provider="openai",
            model="gpt-5.4-nano",
            api_key="openai-key",
        ),
        client=client,
        max_output_tokens=64,
        temperature=0.1,
    )
    messages = [
        {"role": "system", "content": "system prompt"},
        {"role": "user", "content": "user prompt"},
    ]

    result = adapter.invoke_messages(messages)

    assert result == '{"summary":"ok"}'
    assert responses.calls == [
        {
            "model": "gpt-5.4-nano",
            "input": messages,
            "max_output_tokens": 64,
            "temperature": 0.1,
        }
    ]


def test_openai_llm_adapter_returns_response_text() -> None:
    responses = FakeOpenAiResponses()
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-4o-mini",
            api_key="openai-key",
        ),
        client=client,
        timeout_ms=1234,
        max_output_tokens=64,
        temperature=0.1,
    )

    result = adapter.invoke("prompt")

    assert result == '{"summary":"ok"}'
    assert responses.calls[0] == {
        "model": "gpt-4o-mini",
        "input": [{"role": "user", "content": "prompt"}],
        "max_output_tokens": 64,
        "temperature": 0.1,
    }


def test_openai_llm_adapter_uses_max_output_tokens_for_gpt5_models() -> None:
    responses = FakeOpenAiResponses()
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="default",
            provider="openai",
            model="gpt-5.4-nano",
            api_key="openai-key",
        ),
        client=client,
        max_output_tokens=64,
    )

    result = adapter.invoke("prompt")

    assert result == '{"summary":"ok"}'
    assert responses.calls[0]["max_output_tokens"] == 64


def test_openai_llm_adapter_sends_system_and_user_messages() -> None:
    responses = FakeOpenAiResponses()
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-4o-mini",
            api_key="openai-key",
        ),
        client=client,
    )
    messages = [
        {"role": "system", "content": "system prompt"},
        {"role": "user", "content": "user prompt"},
    ]

    result = adapter.invoke_messages(messages)

    assert result == '{"summary":"ok"}'
    assert responses.calls[0]["input"] == messages


def test_openai_llm_adapter_returns_none_without_api_key() -> None:
    responses = FakeOpenAiResponses()
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="",
        ),
        client=client,
    )

    assert adapter.invoke("prompt") is None
    assert responses.calls == []


def test_openai_llm_adapter_returns_none_for_provider_error() -> None:
    responses = FakeOpenAiResponses(error=RuntimeError("provider failed"))
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="openai-key",
        ),
        client=client,
    )

    assert adapter.invoke("prompt") is None


def test_openai_llm_adapter_returns_none_for_empty_response_text() -> None:
    responses = FakeOpenAiResponses(FakeOpenAiResponse("   "))
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="openai-key",
        ),
        client=client,
    )

    assert adapter.invoke("prompt") is None


def test_openai_llm_adapter_preserves_json_object_response_text() -> None:
    responses = FakeOpenAiResponses(
        FakeOpenAiResponse('  {"route":"operator_guide"}\n')
    )
    client = FakeOpenAiResponsesClient(responses)
    adapter = OpenAILLMAdapter(
        LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="openai-key",
        ),
        client=client,
    )

    assert adapter.invoke("prompt") == '  {"route":"operator_guide"}\n'


def test_local_llm_adapter_returns_response_text_without_api_key() -> None:
    http_client = FakeOpenAiHttpClient()
    adapter = LocalLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="local",
            model="llama3.1:8b",
            base_url="http://localhost:11434/v1",
        ),
        http_client=http_client,
        timeout_ms=1234,
        max_output_tokens=64,
        temperature=0.1,
    )

    result = adapter.invoke("prompt")

    assert result == '{"summary":"ok"}'
    assert http_client.calls[0] == {
        "url": "http://localhost:11434/v1/chat/completions",
        "headers": {
            "Content-Type": "application/json",
        },
        "json_body": {
            "model": "llama3.1:8b",
            "messages": [{"role": "user", "content": "prompt"}],
            "max_tokens": 64,
            "temperature": 0.1,
        },
        "timeout_ms": 1234,
    }


def test_local_llm_adapter_sends_system_and_user_messages() -> None:
    http_client = FakeOpenAiHttpClient()
    adapter = LocalLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="local",
            model="llama3.1:8b",
            base_url="http://localhost:11434/v1",
        ),
        http_client=http_client,
    )
    messages = [
        {"role": "system", "content": "system prompt"},
        {"role": "user", "content": "user prompt"},
    ]

    result = adapter.invoke_messages(messages)

    assert result == '{"summary":"ok"}'
    assert http_client.calls[0]["json_body"]["messages"] == messages


def test_local_llm_adapter_returns_none_without_base_url() -> None:
    http_client = FakeOpenAiHttpClient()
    adapter = LocalLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="local",
            model="llama3.1:8b",
        ),
        http_client=http_client,
    )

    assert adapter.invoke("prompt") is None
    assert http_client.calls == []


def test_local_llm_adapter_returns_none_for_endpoint_error() -> None:
    http_client = FakeOpenAiHttpClient(error=RuntimeError("provider failed"))
    adapter = LocalLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="local",
            model="llama3.1:8b",
            base_url="http://localhost:11434/v1",
        ),
        http_client=http_client,
    )

    assert adapter.invoke("prompt") is None


def test_local_llm_adapter_returns_none_for_http_error_response() -> None:
    http_client = FakeOpenAiHttpClient(
        FakeHttpResponse(500, {"error": {"message": "provider failed"}})
    )
    adapter = LocalLLMAdapter(
        LLMModelSlot(
            name="default",
            provider="local",
            model="llama3.1:8b",
            base_url="http://localhost:11434/v1",
        ),
        http_client=http_client,
    )

    assert adapter.invoke("prompt") is None
