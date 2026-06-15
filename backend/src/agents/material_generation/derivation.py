"""입력 재료로부터 물질 속성·category·state·rarity를 결정론적으로 산출합니다.

이 모듈은 합성 장비로 유입된 정규화된 입력 재료와 공정 정보에 기반하여,
재형성될 신물질의 최종 스펙트럼(강도, 전도도, 안정성, 반응성) 및 분류 정보를
완벽히 수학적이고 일관되게 유도(derive)해냅니다.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from agents.material_generation.material_properties import (
    get_base_properties,
)
from agents.material_generation.schemas import (
    MaterialProperties,
    ProcessConditionsSchema,
)


@dataclass
class DerivedAttributes:
    """결정론적으로 산출된 물질의 구조화 속성 묶음입니다."""

    properties: MaterialProperties
    category: str
    state: str
    rarity: str


def _clamp(value: float) -> float:
    """속성 값이 0.0 ~ 10.0 범위를 벗어나지 않도록 강제합니다."""
    return max(0.0, min(10.0, value))


def combine_properties(
    normalized_inputs: list[dict[str, Any]],
) -> tuple[float, float, float, float]:
    """입력 재료들의 기준 속성을 수량 가중 평균으로 합성합니다.

    각 아이템의 수량(qty)을 가중치로 삼아, 강도, 전도도, 안정성, 반응성을
    가중 평균 내어 결정론적인 합성 속성을 반환합니다.
    """
    total_qty = sum(item["qty"] for item in normalized_inputs)
    if total_qty <= 0:
        return (5.0, 5.0, 5.0, 5.0)

    sums = [0.0, 0.0, 0.0, 0.0]
    for item in normalized_inputs:
        props = get_base_properties(item["item_id"])
        for i in range(4):
            sums[i] += props[i] * item["qty"]

    return (
        sums[0] / total_qty,
        sums[1] / total_qty,
        sums[2] / total_qty,
        sums[3] / total_qty,
    )


def apply_process(
    props: tuple[float, float, float, float],
    conditions: ProcessConditionsSchema,
) -> tuple[float, float, float, float]:
    """공정 조건(온도/압력/촉매)에 따라 속성을 보정하고 0~10으로 클램핑합니다.

    - 고온(temperature="high"): 반응성 +1.0, 안정성 -1.0
    - 저온(temperature="low"): 반응성 -1.0, 안정성 +1.0
    - 고압(pressure="high"): 강도 +1.0, 안정성 +1.0
    - 촉매(catalyst): 반응성 +1.0
    """
    strength, conductivity, stability, reactivity = props

    temp = (conditions.temperature or "").strip().lower()
    pressure = (conditions.pressure or "").strip().lower()

    if temp == "high":
        reactivity += 1.0
        stability -= 1.0
    elif temp == "low":
        reactivity -= 1.0
        stability += 1.0

    if pressure == "high":
        strength += 1.0
        stability += 1.0

    if conditions.catalyst:
        reactivity += 1.0

    return (
        _clamp(strength),
        _clamp(conductivity),
        _clamp(stability),
        _clamp(reactivity),
    )
