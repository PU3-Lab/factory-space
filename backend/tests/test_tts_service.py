from __future__ import annotations

import asyncio
from pathlib import Path

import edge_tts
import pytest

from tts.edge_tts_client import EdgeTTSClient
from tts.schemas import TTSRequest
from tts.service import TTSService
from tts.settings import TTSSettings


class FakeProvider:
    provider = "edge_tts"

    def __init__(self) -> None:
        self.calls: list[str] = []

    def synthesize(self, text: str) -> bytes:
        self.calls.append(text)
        return b"mp3-bytes"


def test_tts_settings_dev_defaults_to_edge_tts_without_api_key(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.delenv("ELEVENLABS_API_KEY", raising=False)
    monkeypatch.delenv("FACTORY_TTS_PROVIDER", raising=False)
    monkeypatch.setenv("FACTORY_TTS_ENABLED", "true")
    monkeypatch.setenv("FACTORY_ENV", "development")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is True
    assert settings.provider == "edge_tts"
    assert settings.voice_id == "ko-KR-SunHiNeural"
    assert settings.model_id == "edge_tts"


def test_tts_settings_prod_elevenlabs_requires_api_key(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.delenv("ELEVENLABS_API_KEY", raising=False)
    monkeypatch.delenv("FACTORY_TTS_PROVIDER", raising=False)
    monkeypatch.setenv("FACTORY_TTS_ENABLED", "true")
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.provider == "elevenlabs"
    assert settings.disabled_reason == "missing_api_key"


def test_tts_settings_prod_uses_elevenlabs_defaults(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is True
    assert settings.provider == "elevenlabs"
    assert settings.model_id == "eleven_multilingual_v2"
    assert settings.output_format == "mp3_44100_128"
    assert settings.max_chars == 600


def test_tts_service_returns_disabled_status(tmp_path: Path) -> None:
    settings = TTSSettings(
        api_key=None,
        enabled=False,
        provider="edge_tts",
        storage_path=tmp_path,
    )
    service = TTSService(settings=settings, provider=FakeProvider())

    result = service.synthesize_for_payload(
        TTSRequest(agent="operator_guide", payload={"final_answer": "안내입니다."})
    )

    assert result.status == "disabled"
    assert result.audio_url is None


def test_tts_service_generates_and_caches_audio(tmp_path: Path) -> None:
    provider = FakeProvider()
    settings = TTSSettings(
        api_key=None,
        enabled=True,
        provider="edge_tts",
        voice_id="ko-KR-SunHiNeural",
        model_id="edge_tts",
        storage_path=tmp_path,
    )
    service = TTSService(settings=settings, provider=provider)
    request = TTSRequest(
        agent="operator_guide",
        payload={"final_answer": "안내입니다."},
    )

    first = service.synthesize_for_payload(request)
    second = service.synthesize_for_payload(request)

    assert first.status == "ready"
    assert first.provider == "edge_tts"
    assert first.cached is False
    assert second.status == "ready"
    assert second.cached is True
    assert provider.calls == ["안내입니다."]


def test_tts_settings_production_edge_tts_override(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "edge_tts")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    # 3차 재리뷰 보강: prod/staging 환경에서 edge_tts 강제 지정 시 비활성화되어야 함
    assert settings.enabled is False
    assert settings.disabled_reason == "invalid_provider_for_environment"


def test_tts_settings_development_elevenlabs_override(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("FACTORY_ENV", "development")
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "elevenlabs")
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    # 3차 재리뷰 보강: dev/local/test 환경에서 elevenlabs 강제 지정 시 비활성화되어야 함
    assert settings.enabled is False
    assert settings.disabled_reason == "invalid_provider_for_environment"


def test_tts_settings_unknown_provider_disables_tts(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "unknown-provider-xyz")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.disabled_reason == "invalid_provider"


def test_tts_settings_elevenlabs_invalid_output_format_disables_tts(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("ELEVENLABS_OUTPUT_FORMAT", "pcm_44100")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.disabled_reason == "invalid_output_format"


def test_tts_settings_conflicting_environments(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    # Set conflicting environment variables
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("APP_ENV", "development")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.disabled_reason == "conflicting_environment"


class SlowProvider:
    provider = "edge_tts"

    def synthesize(self, text: str) -> bytes:
        import time
        time.sleep(2.0)
        return b"ok"


def test_tts_service_handles_timeout_error(tmp_path: Path) -> None:
    settings = TTSSettings(
        api_key=None,
        enabled=True,
        provider="edge_tts",
        storage_path=tmp_path,
        timeout_seconds=0.5,
    )
    service = TTSService(settings=settings, provider=SlowProvider())

    request = TTSRequest(
        agent="operator_guide",
        payload={"final_answer": "타임아웃 발생 테스트"},
    )

    import time
    start_time = time.time()
    result = service.synthesize_for_payload(request)
    elapsed_time = time.time() - start_time

    assert result.status == "failed"
    assert result.error_code == "TTS_TIMEOUT"
    # 5차 재리뷰 보강: 느린 provider 호출이 timeout_seconds(0.5초) 바운더리 내에서 정상적으로 끊겼는지 확인
    assert elapsed_time < 1.0


def test_edge_tts_client_timeout_no_running_loop(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    # 6차 재리뷰 보강: no-running-loop 환경에서 edge-tts client의 timeout이 elapsed_time 내에 차단되는지 검증
    async def mock_save(*args: object, **kwargs: object) -> None:
        await asyncio.sleep(5.0)
    monkeypatch.setattr(edge_tts.Communicate, "save", mock_save)

    settings = TTSSettings(
        api_key=None,
        enabled=True,
        provider="edge_tts",
        storage_path=tmp_path,
        timeout_seconds=0.5,
    )
    client = EdgeTTSClient(settings)

    import time
    start_time = time.time()
    with pytest.raises(TimeoutError):
        client.synthesize("테스트")
    elapsed = time.time() - start_time
    assert elapsed < 1.0


@pytest.mark.anyio
async def test_edge_tts_client_timeout_running_loop(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    # 6차 재리뷰 보강: running-loop 환경에서 edge-tts client의 timeout이 elapsed_time 내에 차단되는지 검증
    async def mock_save(*args: object, **kwargs: object) -> None:
        await asyncio.sleep(5.0)
    monkeypatch.setattr(edge_tts.Communicate, "save", mock_save)

    settings = TTSSettings(
        api_key=None,
        enabled=True,
        provider="edge_tts",
        storage_path=tmp_path,
        timeout_seconds=0.5,
    )
    client = EdgeTTSClient(settings)

    import time
    start_time = time.time()
    with pytest.raises(TimeoutError):
        client.synthesize("테스트")
    elapsed = time.time() - start_time
    assert elapsed < 1.0


def test_tts_service_disabled_preserves_error_code(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    # 6차 재리뷰 보강: 비활성화된 settings 사유가 service metadata의 error_code에 보존되는지 검증
    # 1. conflicting_environment 검증
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("APP_ENV", "development")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()
    service = TTSService(settings=settings)

    request = TTSRequest(agent="operator_guide", payload={"question": "테스트"})
    result = service.synthesize_for_payload(request)
    assert result.status == "disabled"
    assert result.error_code == "conflicting_environment"

    # 2. invalid_provider_for_environment 검증
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.delenv("APP_ENV", raising=False)
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "edge_tts")

    settings = TTSSettings.from_env()
    service = TTSService(settings=settings)
    result = service.synthesize_for_payload(request)
    assert result.status == "disabled"
    assert result.error_code == "invalid_provider_for_environment"

