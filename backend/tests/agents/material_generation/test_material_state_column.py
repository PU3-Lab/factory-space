"""GeneratedMaterialModel.state 컬럼 존재 테스트입니다.

이 테스트는 데이터베이스의 GeneratedMaterialModel 테이블 스키마에
새롭게 state(물리적 상태) 필드에 해당하는 nullable=False인 String 컬럼이
정상적으로 정의되었는지 확인합니다.
"""

from __future__ import annotations

from db.models import GeneratedMaterialModel


def test_material_model_has_state_column() -> None:
    """GeneratedMaterialModel 스키마에 state 컬럼이 존재하며, Non-nullable 속성인지 검증합니다."""
    assert "state" in GeneratedMaterialModel.__table__.columns
    col = GeneratedMaterialModel.__table__.columns["state"]
    assert col.nullable is False
