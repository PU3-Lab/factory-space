from __future__ import annotations

from pathlib import Path

import pytest

from tts.storage import TTSAudioStorage


def test_storage_key_is_stable_and_agent_scoped(tmp_path: Path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")

    first = storage.build_key(
        agent="operator_guide",
        text="같은 문장",
        voice_id="voice-a",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
    )
    second = storage.build_key(
        agent="operator_guide",
        text="같은 문장",
        voice_id="voice-a",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
    )

    assert first == second
    assert len(first) == 64
    assert "/" not in first


def test_storage_key_changes_by_voice_variant(tmp_path: Path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")

    default_variant = storage.build_key(
        agent="operator_guide",
        text="같은 문장",
        voice_id="voice-a",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
    )
    android_variant = storage.build_key(
        agent="operator_guide",
        text="같은 문장",
        voice_id="voice-a",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
        voice_variant=(
            "stability=1.0|similarity_boost=1.0|style=0.0|"
            "speed=1.0|use_speaker_boost=true"
        ),
    )

    assert default_variant != android_variant


def test_storage_writes_mp3_under_agent_directory(tmp_path: Path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")
    valid_key = "a" * 64

    result = storage.write_audio(
        agent="operator_guide",
        key=valid_key,
        audio_bytes=b"mp3-bytes",
        extension="mp3",
    )

    assert result.path == tmp_path / "operator_guide" / f"{valid_key}.mp3"
    assert result.path.read_bytes() == b"mp3-bytes"
    assert result.audio_url == f"/tts/operator_guide/{valid_key}.mp3"


def test_storage_rejects_invalid_inputs(tmp_path: Path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")
    valid_key = "a" * 64

    # 1. 지원하지 않는 에이전트 대상 검증
    with pytest.raises(ValueError, match="지원하지 않는 에이전트 대상입니다"):
        storage.write_audio(agent="invalid_agent", key=valid_key, audio_bytes=b"bytes")

    # 2. 64자리 소문자 hex 규격을 만족하지 않는 캐시 키 검증
    invalid_keys = ["abc123", "a" * 63, "a" * 65, "a" * 63 + "g", "a" * 60 + "/../"]
    for key in invalid_keys:
        with pytest.raises(ValueError, match="잘못된 캐시 키 형식입니다"):
            storage.write_audio(agent="operator_guide", key=key, audio_bytes=b"bytes")

    # 3. mp3 이외의 확장자 차단 검증
    with pytest.raises(ValueError, match="지원하지 않는 파일 확장자입니다"):
        storage.write_audio(
            agent="operator_guide", key=valid_key, audio_bytes=b"bytes", extension="wav"
        )
