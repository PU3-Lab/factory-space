"""결정론적 물질 속성 산출 단위 테스트입니다.

이 테스트는 여러 기초 재료가 합성될 때 속성이 수량 가중 평균에 따라
올바르게 계산되는지 검증합니다.
"""

from __future__ import annotations

from agents.material_generation.derivation import combine_properties


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
