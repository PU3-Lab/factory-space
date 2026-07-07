from __future__ import annotations

import json
import urllib.request
from pathlib import Path
from typing import Any

import pytest

from tts.elevenlabs_client import ElevenLabsClient
from tts.service import TTSService
from tts.settings import TTSSettings


class FakeResponse:
    status = 200

    def __enter__(self) -> FakeResponse:
        return self

    def __exit__(self, *args: object) -> None:
        return None

    def read(self) -> bytes:
        return b"mp3-bytes"


def test_prod_tts_service_uses_elevenlabs_client(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    monkeypatch.delenv("APP_ENV", raising=False)
    monkeypatch.delenv("ENVIRONMENT", raising=False)
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "auto")
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    service = TTSService.from_env()

    assert service.settings.provider == "elevenlabs"
    assert isinstance(service.provider, ElevenLabsClient)


def test_elevenlabs_client_sends_stream_request(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    captured: dict[str, Any] = {}

    def fake_urlopen(request: urllib.request.Request, timeout: float) -> FakeResponse:
        captured["url"] = request.full_url
        captured["headers"] = dict(request.header_items())
        captured["body"] = json.loads(request.data.decode("utf-8"))
        captured["timeout"] = timeout
        return FakeResponse()

    monkeypatch.setattr("urllib.request.urlopen", fake_urlopen)
    settings = TTSSettings(
        enabled=True,
        provider="elevenlabs",
        storage_path=tmp_path,
        api_key="test-key",
        voice_id="voice-123",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
        api_base_url="https://api.us.elevenlabs.io",
        timeout_seconds=3.0,
    )

    audio = ElevenLabsClient(settings).synthesize("안내 문장입니다.")

    assert audio == b"mp3-bytes"
    assert captured["url"] == (
        "https://api.us.elevenlabs.io/v1/text-to-speech/voice-123/stream"
        "?output_format=mp3_44100_128"
    )
    assert captured["headers"]["Xi-api-key"] == "test-key"
    assert captured["headers"]["Content-type"] == "application/json"
    assert captured["headers"]["Accept"] == "audio/mpeg"
    assert captured["body"] == {
        "text": "안내 문장입니다.",
        "model_id": "eleven_multilingual_v2",
    }
    assert captured["timeout"] == 3.0


def test_elevenlabs_client_sends_configured_voice_settings(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    captured: dict[str, Any] = {}

    def fake_urlopen(request: urllib.request.Request, timeout: float) -> FakeResponse:
        captured["body"] = json.loads(request.data.decode("utf-8"))
        return FakeResponse()

    monkeypatch.setattr("urllib.request.urlopen", fake_urlopen)
    settings = TTSSettings(
        enabled=True,
        provider="elevenlabs",
        storage_path=tmp_path,
        api_key="test-key",
        voice_id="voice-123",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
        eleven_stability=1.0,
        eleven_similarity_boost=1.0,
        eleven_style=0.0,
        eleven_speed=1.0,
        eleven_use_speaker_boost=True,
    )

    ElevenLabsClient(settings).synthesize("안드로이드 톤 테스트입니다.")

    assert captured["body"]["voice_settings"] == {
        "stability": 1.0,
        "similarity_boost": 1.0,
        "style": 0.0,
        "speed": 1.0,
        "use_speaker_boost": True,
    }
