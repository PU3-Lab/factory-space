from __future__ import annotations

from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from app import create_app


def test_tts_secure_route_serves_valid_file(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    # 64-char hex key
    valid_key = "a" * 64
    agent = "operator_guide"

    # Create the directory structure and file
    audio_dir = tmp_path / agent
    audio_dir.mkdir(parents=True)
    (audio_dir / f"{valid_key}.mp3").write_bytes(b"mp3-bytes")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    client = TestClient(create_app())
    response = client.get(f"/tts/{agent}/{valid_key}.mp3")

    assert response.status_code == 200
    assert response.content == b"mp3-bytes"


def test_tts_secure_route_blocks_invalid_agent(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    valid_key = "a" * 64
    agent = "invalid_agent"

    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))
    client = TestClient(create_app())
    response = client.get(f"/tts/{agent}/{valid_key}.mp3")

    assert response.status_code == 403
    assert "Forbidden agent directory" in response.json()["detail"]


def test_tts_secure_route_blocks_invalid_key_format(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    agent = "operator_guide"
    invalid_keys = ["sample", "a" * 63, "a" * 65, "invalid-char-g" + "a" * 63]

    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))
    client = TestClient(create_app())

    for key in invalid_keys:
        response = client.get(f"/tts/{agent}/{key}.mp3")
        assert response.status_code == 400
        assert "Invalid audio key format" in response.json()["detail"]


def test_tts_secure_route_returns_404_if_not_found(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    valid_key = "b" * 64
    agent = "operator_guide"

    # Ensure folder exists but file does not
    (tmp_path / agent).mkdir(parents=True, exist_ok=True)
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    client = TestClient(create_app())
    response = client.get(f"/tts/{agent}/{valid_key}.mp3")

    assert response.status_code == 404
    assert "Audio file not found" in response.json()["detail"]


def test_tts_secure_route_blocks_path_traversal_attempts(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))
    client = TestClient(create_app())

    # Traversal in agent (FastAPI strips or normalizes, but if it gets through, allowlist blocks it)
    response_agent = client.get("/tts/operator_guide/../operator_guide/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.mp3")
    assert response_agent.status_code == 404
    # Wait, FastAPI normalizes "/operator_guide/../operator_guide/..." to "/operator_guide/...", which serves a valid file or returns 404.
    # But if we URL-encode it:
    response_encoded = client.get("/tts/operator_guide%2F..%2F..%2F/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.mp3")
    assert response_encoded.status_code in {403, 404}  # Rejection (403 or 404 are both safe)

    # Invalid characters or traversal in key
    response_key = client.get("/tts/operator_guide/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/../../.env.mp3")
    assert response_key.status_code in {400, 404}


def test_serve_agent_audio_direct_traversal_validation() -> None:
    from fastapi import HTTPException

    from tts.router import serve_agent_audio

    # Test the direct function validation logic
    with pytest.raises(HTTPException) as exc_agent:
        serve_agent_audio(agent="operator_guide/../", audio_key="a" * 64)
    assert exc_agent.value.status_code == 403

    with pytest.raises(HTTPException) as exc_key:
        serve_agent_audio(agent="operator_guide", audio_key="a" * 64 + "/../../.env")
    assert exc_key.value.status_code == 400

