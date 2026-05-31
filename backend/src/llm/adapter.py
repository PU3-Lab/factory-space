"""LLM provider adapters."""

from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any, Protocol

from llm.settings import LlmModelSlot

_OPENAI_BASE_URL = "https://api.openai.com/v1"


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


class _HttpResponse(Protocol):
    status_code: int
    body: object


class _HttpClient(Protocol):
    def post(
        self,
        *,
        url: str,
        headers: dict[str, str],
        json_body: dict[str, Any],
        timeout_ms: int,
    ) -> _HttpResponse:
        """Send JSON HTTP POST."""


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
    """OpenAI-compatible Chat Completions adapter."""

    slot: LlmModelSlot
    http_client: _HttpClient | None = None
    timeout_ms: int = 20000
    max_output_tokens: int = 2048
    temperature: float = 0.2

    def invoke(self, prompt: str) -> str | None:
        """Return raw generated text from an OpenAI-compatible endpoint."""

        if not self.slot.model or not self.slot.api_key:
            return None
        try:
            http_client = self.http_client or _UrlLibHttpClient()
            response = http_client.post(
                url=_openai_chat_completions_url(self.slot.base_url),
                headers={
                    "Authorization": f"Bearer {self.slot.api_key}",
                    "Content-Type": "application/json",
                },
                json_body={
                    "model": self.slot.model,
                    "messages": [{"role": "user", "content": prompt}],
                    "max_tokens": self.max_output_tokens,
                    "temperature": self.temperature,
                },
                timeout_ms=self.timeout_ms,
            )
        except Exception:
            return None

        if response.status_code < 200 or response.status_code >= 300:
            return None
        return _extract_openai_message_content(response.body)


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


@dataclass(frozen=True)
class _HttpJsonResponse:
    status_code: int
    body: object


class _UrlLibHttpClient:
    def post(
        self,
        *,
        url: str,
        headers: dict[str, str],
        json_body: dict[str, Any],
        timeout_ms: int,
    ) -> _HttpJsonResponse:
        import urllib.error
        import urllib.request

        request = urllib.request.Request(
            url,
            data=json.dumps(json_body).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        try:
            with urllib.request.urlopen(
                request,
                timeout=timeout_ms / 1000,
            ) as response:
                body = json.loads(response.read().decode("utf-8"))
                return _HttpJsonResponse(status_code=response.status, body=body)
        except urllib.error.HTTPError as exc:
            return _HttpJsonResponse(status_code=exc.code, body={})


def _openai_chat_completions_url(base_url: str | None) -> str:
    return f"{(base_url or _OPENAI_BASE_URL).rstrip('/')}/chat/completions"


def _extract_openai_message_content(body: object) -> str | None:
    if not isinstance(body, dict):
        return None
    choices = body.get("choices")
    if not isinstance(choices, list) or not choices:
        return None
    first_choice = choices[0]
    if not isinstance(first_choice, dict):
        return None
    message = first_choice.get("message")
    if not isinstance(message, dict):
        return None
    content = message.get("content")
    if not isinstance(content, str):
        return None
    if not content.strip():
        return None
    return content
