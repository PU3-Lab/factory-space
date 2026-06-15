"""결정론적 물질 속성 산출 단위 테스트입니다.

이 테스트는 여러 기초 재료가 합성될 때 속성이 수량 가중 평균에 따라
올바르게 계산되는지 검증합니다.
"""

from __future__ import annotations

from agents.material_generation.derivation import (
    apply_process,
    combine_properties,
    derive_category,
)
from agents.material_generation.schemas import ProcessConditionsSchema


def test_combine_single_input() -> None:
    """단일 재료 입력 시 해당 재료의 속성이 그대로 도출되는지 테스트합니다."""
    # iron = (7,5,7,4)
    result = combine_properties([{"item_id": "iron_ingot", "qty": 1}])
    assert result == (7.0, 5.0, 7.0, 4.0)


def test_combine_qty_weighted_average() -> None:
    """복수 재료의 수량이 동일할 때 산술 평균으로 도출되는지 테스트합니다."""
    # iron=(7,5,7,4) x1, copper=(4,9,6,4) x1 -> 평균 (5.5,7,6.5,4)
    result = combine_properties(
        [
            {"item_id": "iron_ingot", "qty": 1},
            {"item_id": "copper_ingot", "qty": 1},
        ]
    )
    assert result == (5.5, 7.0, 6.5, 4.0)


def test_combine_respects_quantity_weight() -> None:
    """복수 재료의 수량이 다를 때 수량 가중 평균으로 올바르게 도출되는지 테스트합니다."""
    # iron x3, copper x1 -> strength (7*3+4*1)/4 = 6.25
    result = combine_properties(
        [
            {"item_id": "iron_ingot", "qty": 3},
            {"item_id": "copper_ingot", "qty": 1},
        ]
    )
    assert result[0] == 6.25


def test_apply_process_high_temp() -> None:
    """고온 조건에서 반응성이 증가하고 안정성이 감소하는지 테스트합니다."""
    # base (5,5,5,5), high temp -> reactivity+1, stability-1
    out = apply_process(
        (5.0, 5.0, 5.0, 5.0), ProcessConditionsSchema(temperature="high")
    )
    assert out == (5.0, 5.0, 4.0, 6.0)


def test_apply_process_catalyst_and_pressure() -> None:
    """압력 및 촉매 조건이 반영되는지 테스트합니다."""
    out = apply_process(
        (5.0, 5.0, 5.0, 5.0),
        ProcessConditionsSchema(pressure="high", catalyst="platinum"),
    )
    # strength+1, stability+1, reactivity+1
    assert out == (6.0, 5.0, 6.0, 6.0)


def test_apply_process_clamps() -> None:
    """속성이 0.0 미만 또는 10.0 초과로 나가지 않고 클램핑되는지 테스트합니다."""
    out = apply_process(
        (10.0, 10.0, 0.5, 9.5), ProcessConditionsSchema(temperature="high")
    )
    # stability 0.5-1 -> clamp 0, reactivity 9.5+1 -> clamp 10
    assert out == (10.0, 10.0, 0.0, 10.0)


def test_apply_process_default_is_noop() -> None:
    """기본 공정 조건일 때 속성 변화가 없는지 테스트합니다."""
    out = apply_process((5.0, 5.0, 5.0, 5.0), ProcessConditionsSchema())
    assert out == (5.0, 5.0, 5.0, 5.0)


def test_derive_category_all_metals_is_alloy() -> None:
    """금속 재료들만 섞었을 때 alloy(합금) 카테고리로 판정되는지 테스트합니다."""
    inputs = [
        {"item_id": "iron_ingot", "qty": 1},
        {"item_id": "copper_ingot", "qty": 1},
    ]
    assert derive_category(inputs) == "alloy"


def test_derive_category_organic() -> None:
    """유기물군에 속한 기초 재료들만 섞었을 때 organic(유기물) 카테고리로 판정되는지 테스트합니다."""
    inputs = [{"item_id": "charcoal_dust", "qty": 1}, {"item_id": "wood", "qty": 1}]
    assert derive_category(inputs) == "organic"


def test_derive_category_chemical() -> None:
    """금속이 없고 비금속군(황, 석탄 등)이 섞였을 때 chemical(화학물질) 카테고리로 판정되는지 테스트합니다."""
    inputs = [
        {"item_id": "sulfur_powder", "qty": 1},
        {"item_id": "coal_dust", "qty": 1},
    ]
    assert derive_category(inputs) == "chemical"


def test_derive_category_composite() -> None:
    """금속과 비금속이 혼합되었을 때 composite(복합재) 카테고리로 판정되는지 테스트합니다."""
    inputs = [
        {"item_id": "iron_ingot", "qty": 1},
        {"item_id": "sulfur_powder", "qty": 1},
    ]
    assert derive_category(inputs) == "composite"
