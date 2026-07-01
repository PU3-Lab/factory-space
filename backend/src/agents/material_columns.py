"""물질 응답에 포함할 언리얼 CSV/DataTable 컬럼값을 제공합니다."""

from __future__ import annotations

import csv
from functools import lru_cache
from pathlib import Path


def get_unreal_material_column_values(
    visual_color: str | None = None,
) -> dict[str, str]:
    """언리얼 CSV 연동용 기본 신물질 컬럼값을 새 dict로 반환합니다."""
    return {
        "rowname": "wood",
        "form": "solid",
        "substance": "wood",
        "type": "organism",
        "shape": "",
        "DIsplayName": "목재",
        "VisualColor": visual_color or "(R=0.49,G=0.31,B=0.16,A=1.0)",
    }


_CATEGORY_TYPE_SHAPE: dict[str, tuple[str, str]] = {
    "alloy": ("metal", "ingot"),
    "organic": ("organism", ""),
    "chemical": ("chemical", ""),
    "composite": ("composite", ""),
}
_DEFAULT_NEW_MATERIAL_VISUAL_COLOR = "(R=0.50,G=0.50,B=0.50,A=1.0)"


def get_new_material_column_values(
    *,
    id_hint: str,
    name: str,
    category: str,
    state: str,
    visual_color: str | None = None,
) -> dict[str, str]:
    """합성으로 새로 발견된 물질(합금/유기물 등)의 언리얼 DataTable 컬럼값을 산출합니다."""
    material_type, shape = _CATEGORY_TYPE_SHAPE.get(category, ("", ""))
    return {
        "rowname": id_hint,
        "form": state,
        "substance": id_hint,
        "type": material_type,
        "shape": shape,
        "DIsplayName": name,
        "VisualColor": visual_color or _DEFAULT_NEW_MATERIAL_VISUAL_COLOR,
    }


def get_resource_material_column_values(item_id: str) -> dict[str, str]:
    """기존 resources.csv 자원 ID에 맞는 언리얼 DataTable 컬럼값을 반환합니다."""
    row = _resource_rows_by_item_id().get(_normalize_item_id(item_id))
    if row is None:
        return {}

    material_type, shape = _split_resource_type(row.get("자원타입", ""))
    return {
        "rowname": row.get("원본이름", ""),
        "form": _normalize_form(row.get("형태", "")),
        "substance": row.get("물질계열", ""),
        "type": material_type,
        "shape": shape,
        "DIsplayName": row.get("자원명", ""),
        "VisualColor": row.get("시각색상", ""),
    }


@lru_cache(maxsize=1)
def _resource_rows_by_item_id() -> dict[str, dict[str, str]]:
    resources_path = (
        Path(__file__).resolve().parents[3] / "data" / "game" / "resources.csv"
    )
    if not resources_path.exists():
        return {}

    rows: dict[str, dict[str, str]] = {}
    with resources_path.open(encoding="utf-8-sig", newline="") as handle:
        for row in csv.DictReader(handle):
            original_name = row.get("원본이름", "")
            resource_id = row.get("자원ID", "")
            if original_name:
                rows[_normalize_item_id(original_name)] = row
            if resource_id:
                rows[_normalize_item_id(resource_id)] = row
    return rows


def _normalize_item_id(item_id: str) -> str:
    return item_id.removeprefix("resource_")


def _normalize_form(form: str) -> str:
    return {
        "고체": "solid",
        "액체": "liquid",
        "기체": "gas",
        "플라즈마": "plasma",
    }.get(form, form)


def _split_resource_type(resource_type: str) -> tuple[str, str]:
    type_map = {
        "금속": "metal",
        "비금속": "nonmetal",
        "유기물": "organism",
        "연료": "fuel",
        "액체": "liquid",
        "기체": "gas",
        "전력": "power",
        "기계": "machine",
    }
    shape_map = {
        "광석": "ore",
        "주괴": "ingot",
        "분말": "powder",
        "막대": "rod",
        "선": "wire",
        "판": "plate",
    }

    tokens = resource_type.split()
    material_type = type_map.get(tokens[0], tokens[0] if tokens else "")
    shape = ""
    for token in tokens[1:]:
        if token in shape_map:
            shape = shape_map[token]
            break
    return material_type, shape
