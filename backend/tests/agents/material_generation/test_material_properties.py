"""기초 재료 속성 테이블 조회 단위 테스트입니다.

이 테스트는 각 기초 재료의 고유 물리 속성 테이블이 잘 매핑되는지,
재료 ID에서 접미사(_ingot, _powder 등)가 올바르게 제거되는지 확인합니다.
"""

from __future__ import annotations

from agents.material_generation.material_properties import (
    METALS,
    NEUTRAL_PROPERTIES,
    base_key,
    get_base_properties,
)


def test_base_key_strips_known_suffixes() -> None:
    """재료 식별자에서 형태 접미사가 잘 제거되는지 테스트합니다."""
    assert base_key("iron_ingot") == "iron"
    assert base_key("coal_dust") == "coal"
    assert base_key("iron_ore") == "iron"
    assert base_key("copper_powder") == "copper"
    assert base_key("sulfur") == "sulfur"


def test_get_base_properties_returns_table_value() -> None:
    """테이블에 등록된 기초 재료의 4대 속성(강도, 전도도, 안정성, 반응성)을 올바르게 가져오는지 테스트합니다."""
    assert get_base_properties("tungsten_ingot") == (10.0, 4.0, 9.0, 1.0)
    assert get_base_properties("gold_powder") == (2.0, 10.0, 10.0, 1.0)


def test_get_base_properties_unknown_is_neutral() -> None:
    """테이블에 없는 미지의 재료에 대해 중립 속성값(5.0, 5.0, 5.0, 5.0)을 반환하는지 테스트합니다."""
    assert get_base_properties("unobtainium_ingot") == NEUTRAL_PROPERTIES


def test_metals_membership() -> None:
    """특정 원소가 금속군(METALS)에 속해 있는지 분류 체계를 테스트합니다."""
    assert "iron" in METALS
    assert "sulfur" not in METALS
