"""TTS 기능의 런타임 설정을 담당하는 모듈.

초보자용 설명:
    이 모듈은 환경 변수에서 TTS 작동 방식(사용 여부, 공급자, 목소리 종류, API 키 등)을 설정합니다.
    클라이언트와 서버가 주고받을 오디오의 최대 글자 수나 만료 시간을 제한하여 보안과 리소스를 관리합니다.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import NamedTuple


def _parse_bool(val: str | None) -> bool:
    """대소문자 구분 없이 1, true, yes, on을 True로 판별합니다."""
    if not val:
        return False
    return val.strip().lower() in {"1", "true", "yes", "on"}


class TTSSettings(NamedTuple):
    """TTS 설정값을 안전하게 보관하는 불변 데이터 구조체."""

    enabled: bool
    provider: str  # "edge_tts" 또는 "elevenlabs"
    storage_path: Path
    public_base_url: str = "/tts"

    # edge-tts 관련 설정
    voice_id: str = "ko-KR-SunHiNeural"
    edge_rate: str = "+0%"
    edge_volume: str = "+0%"
    edge_pitch: str = "+0Hz"

    # ElevenLabs 관련 설정
    api_key: str | None = None
    api_base_url: str = "https://api.elevenlabs.io"
    model_id: str = "edge_tts"
    output_format: str = "mp3_44100_128"
    eleven_stability: float | None = None
    eleven_similarity_boost: float | None = None
    eleven_style: float | None = None
    eleven_speed: float | None = None
    eleven_use_speaker_boost: bool | None = None

    # 공통 제한 설정
    max_chars: int = 600
    timeout_seconds: float = 2.0

    # 안드로이드 기계음 후처리(링 변조 + 로우패스 + loudnorm) 사용 여부
    robotic_effect_enabled: bool = False

    # 비활성화된 경우의 원인 추적용
    disabled_reason: str | None = None

    @classmethod
    def from_env(cls) -> TTSSettings:
        """환경 변수를 파싱하고 기본값을 적용하여 설정을 빌드합니다."""

        # 1. 활성화 여부 판별 (기본값 True)
        val = os.environ.get("FACTORY_TTS_ENABLED")
        enabled = _parse_bool(val) if val is not None else True

        # 2. 실행 환경 확인 (FACTORY_ENV -> APP_ENV -> ENVIRONMENT 순서로 체크)
        env_vars = {}
        for key in ("FACTORY_ENV", "APP_ENV", "ENVIRONMENT"):
            val = os.environ.get(key)
            if val is not None:
                val_clean = val.strip().lower()
                if val_clean in {"dev", "development", "local", "test"}:
                    env_vars[key] = "dev"
                elif val_clean in {"prod", "production", "staging"}:
                    env_vars[key] = "prod"
                else:
                    env_vars[key] = "dev"

        disabled_reason = None
        if len(set(env_vars.values())) > 1:
            enabled = False
            disabled_reason = "conflicting_environment"
            is_dev = True  # Safe fallback default
        else:
            env_val = (
                (
                    os.environ.get("FACTORY_ENV")
                    or os.environ.get("APP_ENV")
                    or os.environ.get("ENVIRONMENT")
                    or "development"
                )
                .strip()
                .lower()
            )
            is_dev = env_val in {"dev", "development", "local", "test"}

        # 3. Provider 결정 (FACTORY_TTS_PROVIDER가 auto이면 환경에 따라 자동 매핑)
        provider_val = (
            (os.environ.get("FACTORY_TTS_PROVIDER") or "auto").strip().lower()
        )
        if disabled_reason is None:
            if provider_val == "auto":
                provider = "edge_tts" if is_dev else "elevenlabs"
            elif provider_val == "edge_tts":
                provider = "edge_tts"
                if not is_dev:
                    # 3차 재리뷰 보강: prod/staging 환경에서 edge_tts 강제 지정 시 비활성화
                    enabled = False
                    disabled_reason = "invalid_provider_for_environment"
            elif provider_val in {"elevenlabs", "eleven-labs"}:
                provider = "elevenlabs"
                if is_dev:
                    # 3차 재리뷰 보강: dev/local/test 환경에서 elevenlabs 강제 지정 시 비활성화
                    enabled = False
                    disabled_reason = "invalid_provider_for_environment"
            else:
                # 알 수 없는 프로바이더인 경우 비활성화하고 'invalid_provider' 상태 부여
                enabled = False
                provider = "edge_tts"  # 안전한 fallback 할당
                disabled_reason = "invalid_provider"
        else:
            provider = "edge_tts"

        # 4. ElevenLabs API 키 확인 및 예외 처리
        api_key = os.environ.get("ELEVENLABS_API_KEY")

        if enabled and provider == "elevenlabs" and not api_key:
            enabled = False
            disabled_reason = "missing_api_key"

        # 5. 저장 경로 결정
        storage_path_str = os.environ.get("FACTORY_TTS_STORAGE_PATH") or "var/tts"
        storage_path = Path(storage_path_str).resolve()

        # 6. 공통 제한값 설정 (클램핑 처리)
        try:
            max_chars = int(os.environ.get("FACTORY_TTS_MAX_CHARS") or "600")
        except ValueError:
            max_chars = 600
        max_chars = max(100, min(max_chars, 1200))

        try:
            timeout_seconds = float(
                os.environ.get("FACTORY_TTS_TIMEOUT_SECONDS") or "2.0"
            )
        except ValueError:
            timeout_seconds = 2.0
        timeout_seconds = max(0.5, min(timeout_seconds, 8.0))

        # 7. 안드로이드 기계음 후처리 사용 여부 (기본값 False)
        robotic_effect_val = os.environ.get("FACTORY_TTS_ROBOTIC_EFFECT_ENABLED")
        robotic_effect_enabled = (
            _parse_bool(robotic_effect_val) if robotic_effect_val is not None else False
        )

        # 8. edge-tts 목소리 설정
        edge_voice = os.environ.get("EDGE_TTS_VOICE") or "ko-KR-SunHiNeural"
        edge_rate = os.environ.get("EDGE_TTS_RATE") or "+0%"
        edge_volume = os.environ.get("EDGE_TTS_VOLUME") or "+0%"
        edge_pitch = os.environ.get("EDGE_TTS_PITCH") or "+0Hz"

        # 9. ElevenLabs 목소리 및 모델 설정
        eleven_voice = os.environ.get("ELEVENLABS_VOICE_ID") or "JBFqnCBsd6RMkjVDRZzb"
        eleven_model = os.environ.get("ELEVENLABS_MODEL_ID") or "eleven_multilingual_v2"
        eleven_format = os.environ.get("ELEVENLABS_OUTPUT_FORMAT") or "mp3_44100_128"
        eleven_api_base_url = (
            os.environ.get("ELEVENLABS_API_BASE_URL") or "https://api.elevenlabs.io"
        ).rstrip("/")
        eleven_stability = _parse_optional_float("ELEVENLABS_STABILITY", 0.0, 1.0)
        eleven_similarity_boost = _parse_optional_float(
            "ELEVENLABS_SIMILARITY_BOOST",
            0.0,
            1.0,
        )
        eleven_style = _parse_optional_float("ELEVENLABS_STYLE", 0.0, 1.0)
        eleven_speed = _parse_optional_float("ELEVENLABS_SPEED", 0.7, 1.2)
        eleven_use_speaker_boost = _parse_optional_bool("ELEVENLABS_USE_SPEAKER_BOOST")

        # 2차 재리뷰 추가: ElevenLabs output_format 검증 (mp3_*로 시작하지 않으면 invalid_output_format으로 비활성화)
        if (
            enabled
            and provider == "elevenlabs"
            and not eleven_format.startswith("mp3_")
        ):
            enabled = False
            disabled_reason = "invalid_output_format"

        voice_id = eleven_voice if provider == "elevenlabs" else edge_voice
        model_id = eleven_model if provider == "elevenlabs" else "edge_tts"

        return cls(
            enabled=enabled,
            provider=provider,
            storage_path=storage_path,
            public_base_url=os.environ.get("FACTORY_TTS_PUBLIC_BASE_URL") or "/tts",
            voice_id=voice_id,
            edge_rate=edge_rate,
            edge_volume=edge_volume,
            edge_pitch=edge_pitch,
            api_key=api_key,
            api_base_url=eleven_api_base_url,
            model_id=model_id,
            output_format=eleven_format,
            eleven_stability=eleven_stability,
            eleven_similarity_boost=eleven_similarity_boost,
            eleven_style=eleven_style,
            eleven_speed=eleven_speed,
            eleven_use_speaker_boost=eleven_use_speaker_boost,
            max_chars=max_chars,
            timeout_seconds=timeout_seconds,
            robotic_effect_enabled=robotic_effect_enabled,
            disabled_reason=disabled_reason,
        )


def _parse_optional_bool(key: str) -> bool | None:
    val = os.environ.get(key)
    if val is None or val.strip() == "":
        return None
    return _parse_bool(val)


def _parse_optional_float(key: str, min_value: float, max_value: float) -> float | None:
    val = os.environ.get(key)
    if val is None or val.strip() == "":
        return None
    try:
        parsed = float(val)
    except ValueError:
        return None
    return max(min_value, min(parsed, max_value))
