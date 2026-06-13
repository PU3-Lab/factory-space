"""Input normalization and hashing logic for material generation."""

from __future__ import annotations

import hashlib
from typing import Any

from agents.material_generation.schemas import (
    InputItemSchema,
    MaterialProposalResult,
    ProcessConditionsSchema,
)


def normalize_inputs(inputs: list[InputItemSchema]) -> list[dict[str, Any]]:
    """Normalize input items by merging duplicate item_ids, summing quantities, and sorting by item_id."""
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
    """Generate a stable sha256 hash for an experiment combination."""
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
    """Generate a stable sha256 hash for a generated material's properties."""
    # Round properties to 1 decimal place to tolerate minor LLM balance score variations
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
