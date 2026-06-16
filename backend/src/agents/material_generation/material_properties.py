"""기초 재료의 기준 속성 테이블 및 조회 유틸리티입니다.

이 모듈은 새로운 신물질이 생성될 때 입력되는 기초 재료들의 원시 물리적 특성을 정의합니다.
각 기초 재료는 강도(strength), 전도도(conductivity), 안정성(stability), 반응성(reactivity)이라는
4대 속성 스펙트럼(0.0 ~ 10.0 범위)을 가지며, 이를 통해 가중 평균 조합 등의 결정론적 합성이 수행됩니다.
또한 재료 분류(금속군/유기물군) 및 형태 접미사(_ingot, _powder 등)의 정규화를 지원합니다.
"""

from __future__ import annotations

# (strength, conductivity, stability, reactivity), 각 0.0~10.0
BASE_MATERIAL_PROPERTIES: dict[str, tuple[float, float, float, float]] = {
    "iron": (7.0, 5.0, 7.0, 4.0),
    "copper": (4.0, 9.0, 6.0, 4.0),
    "zinc": (3.0, 5.0, 5.0, 6.0),
    "lead": (2.0, 4.0, 6.0, 3.0),
    "tin": (3.0, 5.0, 6.0, 3.0),
    "aluminum": (4.0, 8.0, 6.0, 5.0),
    "nickel": (6.0, 5.0, 7.0, 3.0),
    "tungsten": (10.0, 4.0, 9.0, 1.0),
    "titanium": (8.0, 3.0, 9.0, 2.0),
    "magnesium": (3.0, 6.0, 4.0, 8.0),
    "gold": (2.0, 10.0, 10.0, 1.0),
    "silver": (3.0, 10.0, 8.0, 2.0),
    "charcoal": (1.0, 2.0, 4.0, 6.0),
    "coal": (1.0, 1.0, 4.0, 6.0),
    "sulfur": (1.0, 1.0, 3.0, 8.0),
    "wood": (1.0, 1.0, 3.0, 4.0),
}

NEUTRAL_PROPERTIES: tuple[float, float, float, float] = (5.0, 5.0, 5.0, 5.0)

METALS: frozenset[str] = frozenset(
    {
        "iron",
        "copper",
        "zinc",
        "lead",
        "tin",
        "aluminum",
        "nickel",
        "tungsten",
        "titanium",
        "magnesium",
        "gold",
        "silver",
    }
)

# 탄소·목재 계열(유기물군)
ORGANIC_GROUP: frozenset[str] = frozenset({"charcoal", "coal", "wood"})

_SUFFIXES: tuple[str, ...] = ("_ingot", "_powder", "_dust", "_ore")


def base_key(item_id: str) -> str:
    """item_id에서 형태 접미사(_ingot/_powder/_dust/_ore)를 제거한 기초 원소 키를 반환합니다.

    예: 'iron_ingot' -> 'iron', 'coal_dust' -> 'coal'
    """
    key = item_id.strip().lower()
    for suffix in _SUFFIXES:
        if key.endswith(suffix):
            return key[: -len(suffix)]
    return key


def get_base_properties(item_id: str) -> tuple[float, float, float, float]:
    """기초 재료의 기준 속성을 반환합니다. 테이블에 없으면 중립값(5.0, 5.0, 5.0, 5.0)을 반환합니다.

    각 원소의 스펙트럼 튜플은 (강도, 전도도, 안정성, 반응성) 순서입니다.
    """
    return BASE_MATERIAL_PROPERTIES.get(base_key(item_id), NEUTRAL_PROPERTIES)
