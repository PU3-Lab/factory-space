"""재료 생성을 위한 입력 정규화 및 해싱 로직입니다."""

from __future__ import annotations

import hashlib
from typing import Any

from agents.material_generation.schemas import (
    InputItemSchema,
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


def generate_material_hash(
    machine_type: str,
    normalized_inputs: list[dict[str, Any]],
    process_conditions: ProcessConditionsSchema,
) -> str:
    """합성 정체성(장비+정규화 입력+공정조건)에 대한 결정론적 sha256 해시를 생성합니다."""
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
