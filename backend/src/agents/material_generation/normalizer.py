"""재료 생성을 위한 입력 정규화 및 해싱 로직입니다."""

from __future__ import annotations

import hashlib
from typing import Any

from agents.material_generation.schemas import (
    InputItemSchema,
    MaterialProposalResult,
    ProcessConditionsSchema,
)


def normalize_inputs(inputs: list[InputItemSchema]) -> list[dict[str, Any]]:
    """중복된 item_id를 병합하고 수량을 합산한 뒤, item_id 기준으로 정렬하여 입력 아이템을 정규화합니다."""
    merged: dict[str, int] = {}
    for item in inputs:
        merged[item.item_id] = merged.get(item.item_id, 0) + item.qty

    sorted_keys = sorted(merged.keys())
    return [{"item_id": k, "qty": merged[k]} for k in sorted_keys]


def generate_experiment_hash(
    machine_type: str,
    normalized_inputs: list[dict[str, Any]],
    process_conditions: ProcessConditionsSchema,
) -> str:
    """실험 조합에 대한 고유하고 안정적인 sha256 해시를 생성합니다."""
    inputs_str = "|".join(
        f"{item['item_id']}:{item['qty']}" for item in normalized_inputs
    )
    catalyst_str = process_conditions.catalyst or "none"

    hash_payload = (
        f"{machine_type}|{inputs_str}|"
        f"temp:{process_conditions.temperature}|"
        f"pressure:{process_conditions.pressure}|"
        f"catalyst:{catalyst_str}"
    )

    return hashlib.sha256(hash_payload.encode("utf-8")).hexdigest()


def generate_material_hash(result: MaterialProposalResult) -> str:
    """생성된 재료의 속성에 대한 고유하고 안정적인 sha256 해시를 생성합니다."""
    # 미세한 LLM 밸런스 점수 변동을 수용하기 위해 속성 값을 소수점 첫째 자리에서 반올림합니다.
    props_str = (
        f"strength:{result.properties.strength:.1f}|"
        f"conductivity:{result.properties.conductivity:.1f}|"
        f"stability:{result.properties.stability:.1f}|"
        f"reactivity:{result.properties.reactivity:.1f}"
    )

    risks_str = ",".join(sorted(r.strip().lower() for r in result.risks))
    usage_str = ",".join(sorted(u.strip().lower() for u in result.usage))

    hash_payload = (
        f"category:{result.category.strip().lower()}|"
        f"rarity:{result.rarity.strip().lower()}|"
        f"props:{props_str}|"
        f"risks:{risks_str}|"
        f"usage:{usage_str}"
    )

    return hashlib.sha256(hash_payload.encode("utf-8")).hexdigest()
