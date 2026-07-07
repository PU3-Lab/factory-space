from __future__ import annotations

import shutil
import subprocess

import pytest

from tts.audio_postprocess import AudioPostprocessError, apply_robotic_effect


def _generate_test_tone_mp3(duration_seconds: float = 1.0) -> bytes:
    result = subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            "-f",
            "lavfi",
            "-i",
            f"sine=frequency=440:duration={duration_seconds}",
            "-f",
            "mp3",
            "pipe:1",
        ],
        capture_output=True,
        check=True,
    )
    return result.stdout


@pytest.mark.skipif(
    shutil.which("ffmpeg") is None, reason="ffmpeg가 설치되어 있지 않습니다."
)
def test_apply_robotic_effect_returns_different_nonempty_mp3_bytes() -> None:
    original = _generate_test_tone_mp3()

    processed = apply_robotic_effect(original)

    assert isinstance(processed, bytes)
    assert len(processed) > 0
    assert processed != original


def test_apply_robotic_effect_raises_when_ffmpeg_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def _raise_missing(*args: object, **kwargs: object) -> None:
        raise FileNotFoundError("ffmpeg not found")

    monkeypatch.setattr(subprocess, "run", _raise_missing)

    with pytest.raises(AudioPostprocessError):
        apply_robotic_effect(b"fake-mp3-bytes")


def test_apply_robotic_effect_raises_on_nonzero_returncode(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    class FakeResult:
        returncode = 1
        stdout = b""
        stderr = b"invalid data found when processing input"

    monkeypatch.setattr(subprocess, "run", lambda *args, **kwargs: FakeResult())

    with pytest.raises(AudioPostprocessError):
        apply_robotic_effect(b"fake-mp3-bytes")


def test_apply_robotic_effect_raises_on_timeout(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def _raise_timeout(*args: object, **kwargs: object) -> None:
        raise subprocess.TimeoutExpired(cmd="ffmpeg", timeout=1.0)

    monkeypatch.setattr(subprocess, "run", _raise_timeout)

    with pytest.raises(AudioPostprocessError):
        apply_robotic_effect(b"fake-mp3-bytes", timeout_seconds=1.0)
