"""TTS로 합성된 오디오를 안드로이드 기계음 느낌으로 후처리하는 모듈.

초보자용 설명:
    사람 목소리 mp3에 사인파 캐리어를 곱해 링 변조(ring modulation)로 금속성 버즈를 더하고,
    로우패스 필터로 고음역을 깎아 먹먹한 느낌을 준 뒤, loudnorm으로 음량을 정규화합니다.
    실제 신호 처리는 ffmpeg 외부 프로세스에 위임하며, ffmpeg 실행이 실패하면
    호출부가 원본 오디오로 대체(fail-open)할 수 있도록 AudioPostprocessError를 발생시킵니다.
"""

from __future__ import annotations

import subprocess

RING_MODULATOR_CARRIER_HZ = 45
RING_MODULATOR_SAMPLE_RATE = 48000
MUFFLE_LOWPASS_CUTOFF_HZ = 2200
LOUDNORM_TARGET_LUFS = -16
LOUDNORM_TRUE_PEAK_DB = -1.5
LOUDNORM_LOUDNESS_RANGE = 11
DEFAULT_TIMEOUT_SECONDS = 5.0


class AudioPostprocessError(Exception):
    """오디오 후처리(ffmpeg 실행)가 실패했을 때 발생하는 예외."""


def _build_filter_complex() -> str:
    return (
        f"[0:a]aformat=sample_rates={RING_MODULATOR_SAMPLE_RATE}:channel_layouts=mono[dry];"
        f"[1:a]aformat=sample_rates={RING_MODULATOR_SAMPLE_RATE}:channel_layouts=mono[carrier];"
        "[dry][carrier]amultiply,"
        f"lowpass=f={MUFFLE_LOWPASS_CUTOFF_HZ},"
        f"loudnorm=I={LOUDNORM_TARGET_LUFS}:TP={LOUDNORM_TRUE_PEAK_DB}:LRA={LOUDNORM_LOUDNESS_RANGE}"
        "[wet]"
    )


def apply_robotic_effect(
    audio_bytes: bytes, *, timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS
) -> bytes:
    """mp3 오디오 바이트에 링 변조 + 로우패스 + 음량 정규화를 적용한 새 mp3 바이트를 반환합니다."""
    command = [
        "ffmpeg",
        "-y",
        "-v",
        "error",
        "-i",
        "pipe:0",
        "-f",
        "lavfi",
        "-i",
        f"sine=frequency={RING_MODULATOR_CARRIER_HZ}:sample_rate={RING_MODULATOR_SAMPLE_RATE}",
        "-filter_complex",
        _build_filter_complex(),
        "-map",
        "[wet]",
        "-f",
        "mp3",
        "pipe:1",
    ]

    try:
        result = subprocess.run(
            command,
            input=audio_bytes,
            capture_output=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise AudioPostprocessError("ffmpeg 처리 시간이 초과되었습니다.") from exc
    except OSError as exc:
        raise AudioPostprocessError(
            f"ffmpeg 실행 중 오류가 발생했습니다: {exc}"
        ) from exc

    if result.returncode != 0 or not result.stdout:
        stderr_text = result.stderr.decode("utf-8", errors="replace")
        raise AudioPostprocessError(f"ffmpeg 오디오 후처리 실패: {stderr_text}")

    return result.stdout
