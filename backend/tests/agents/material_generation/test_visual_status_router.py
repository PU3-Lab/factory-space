"""material visual status API asset key 응답 통합 테스트입니다.

이 테스트는 REST API 엔드포인트 `GET /api/v1/materials/{material_id}/visual-status`가
이미 완료된 이미지 에셋 키(visual_asset_key, texture_asset_key, thumbnail_asset_key)의
계약을 온전히 보장하고 올바른 형식의 JSON 응답을 반환하는지 검증합니다.
"""

from __future__ import annotations

from collections.abc import Iterator
from contextlib import contextmanager
from unittest.mock import patch

from fastapi.testclient import TestClient
from sqlalchemy.orm import Session

from app import create_app
from db.models import GeneratedMaterialModel


def test_visual_status_returns_asset_keys(db_session: Session) -> None:
    """DB에 완료된 비주얼 에셋 키 정보가 존재할 때, REST API가 이를 정확히 응답 형태로 변환하는지 검증합니다."""
    db_session.add(
        GeneratedMaterialModel(
            id="mat_ready_001",
            material_hash="hash_mat_ready_001",
            name="Ready Alloy",
            category="alloy",
            rarity="common",
            properties_json={},
            visual_status="visual_ready",
            visual_asset_key="materials/mat_ready_001/icon.png",
            texture_asset_key="materials/mat_ready_001/texture.png",
            thumbnail_asset_key="materials/mat_ready_001/thumbnail.png",
            fallback_icon="materials/default/alloy.png",
        )
    )
    db_session.commit()

    @contextmanager
    def get_test_db_session() -> Iterator[Session]:
        yield db_session

    with patch("agents.material_generation.router.get_db_session", get_test_db_session):
        with TestClient(create_app()) as client:
            response = client.get(
                "/api/v1/materials/mat_ready_001/visual-status"
            )

    assert response.status_code == 200
    assert response.json() == {
        "material_id": "mat_ready_001",
        "visual_status": "visual_ready",
        "visual_asset_key": "materials/mat_ready_001/icon.png",
        "texture_asset_key": "materials/mat_ready_001/texture.png",
        "thumbnail_asset_key": "materials/mat_ready_001/thumbnail.png",
    }
