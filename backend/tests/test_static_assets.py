from __future__ import annotations

import io
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from app import create_app


def _write_png(path: Path, color: str = "blue") -> None:
    from PIL import Image

    path.parent.mkdir(parents=True, exist_ok=True)
    buffer = io.BytesIO()
    Image.new("RGB", (32, 32), color).save(buffer, format="PNG")
    path.write_bytes(buffer.getvalue())


def test_material_image_served_statically(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("FACTORY_IMAGE_STORAGE_PATH", str(tmp_path))
    # create_app의 파이프라인 초기화가 누출된 LLM env에 의존하지 않도록 고정한다.
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")
    # 저장소 키 형식: materials/{id}/icon.png
    _write_png(tmp_path / "materials" / "mat_static_1" / "icon.png")

    with TestClient(create_app()) as client:
        response = client.get("/materials/mat_static_1/icon.png")

    assert response.status_code == 200
    assert response.headers["content-type"] == "image/png"


def test_missing_material_image_returns_404(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("FACTORY_IMAGE_STORAGE_PATH", str(tmp_path))
    # create_app의 파이프라인 초기화가 누출된 LLM env에 의존하지 않도록 고정한다.
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")

    with TestClient(create_app()) as client:
        response = client.get("/materials/mat_absent/icon.png")

    assert response.status_code == 404


def test_static_mount_does_not_break_visual_status_endpoint(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # /materials 정적 마운트가 /api/v1/materials/{id}/visual-status 라우트를 가리지 않아야 한다.
    monkeypatch.setenv("FACTORY_IMAGE_STORAGE_PATH", str(tmp_path))
    # create_app의 파이프라인 초기화가 누출된 LLM env에 의존하지 않도록 고정한다.
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")

    with TestClient(create_app()) as client:
        response = client.get("/api/v1/materials/mat_absent/visual-status")

    # 존재하지 않는 물질이라 404(정적 마운트로 잘못 라우팅되면 다른 결과가 나온다).
    assert response.status_code == 404
