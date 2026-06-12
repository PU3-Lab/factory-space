"""Unit tests for input normalizer and hashing functions."""

from __future__ import annotations

from agents.material_generation.normalizer import (
    generate_experiment_hash,
    generate_material_hash,
    normalize_inputs,
)
from agents.material_generation.schemas import (
    InputItemSchema,
    MaterialProperties,
    MaterialProposalResult,
    ProcessConditionsSchema,
)


def test_normalize_inputs_sorts_correctly() -> None:
    inputs = [
        InputItemSchema(item_id="copper_ingot", qty=2),
        InputItemSchema(item_id="iron_ingot", qty=1),
    ]
    normalized = normalize_inputs(inputs)

    assert normalized[0]["item_id"] == "copper_ingot"
    assert normalized[0]["qty"] == 2
    assert normalized[1]["item_id"] == "iron_ingot"
    assert normalized[1]["qty"] == 1


def test_experiment_hash_is_stable_and_order_independent() -> None:
    conds = ProcessConditionsSchema(
        temperature="default", pressure="default", catalyst=None
    )

    raw_inputs1 = [
        InputItemSchema(item_id="copper_ingot", qty=1),
        InputItemSchema(item_id="iron_ingot", qty=2),
    ]
    raw_inputs2 = [
        InputItemSchema(item_id="iron_ingot", qty=2),
        InputItemSchema(item_id="copper_ingot", qty=1),
    ]

    inputs1 = normalize_inputs(raw_inputs1)
    inputs2 = normalize_inputs(raw_inputs2)

    hash1 = generate_experiment_hash("Synthesizer", inputs1, conds)
    hash2 = generate_experiment_hash("Synthesizer", inputs2, conds)

    assert hash1 == hash2


def test_normalize_inputs_merges_duplicates() -> None:
    inputs = [
        InputItemSchema(item_id="iron_ingot", qty=1),
        InputItemSchema(item_id="copper_ingot", qty=3),
        InputItemSchema(item_id="iron_ingot", qty=2),
    ]
    normalized = normalize_inputs(inputs)

    assert len(normalized) == 2
    assert normalized[0]["item_id"] == "copper_ingot"
    assert normalized[0]["qty"] == 3
    assert normalized[1]["item_id"] == "iron_ingot"
    assert normalized[1]["qty"] == 3


def test_material_hash_is_stable() -> None:
    proposal1 = MaterialProposalResult(
        id_hint="alloy_iron_copper",
        name="Iron Copper Alloy",
        category="alloy",
        rarity="common",
        description="A simple alloy.",
        properties=MaterialProperties(
            strength=6.5, conductivity=5.8, stability=7.2, reactivity=2.1
        ),
        risks=["oxidation"],
        usage=["wire", "frame"],
        next_recipe_candidates=["reinforced_wire"],
        visual_prompt="iron copper alloy ingot",
    )

    proposal2 = MaterialProposalResult(
        id_hint="alloy_iron_copper",
        name="Iron Copper Alloy",
        category="alloy",
        rarity="common",
        description="A simple alloy.",
        properties=MaterialProperties(
            strength=6.5, conductivity=5.8, stability=7.2, reactivity=2.1
        ),
        risks=["oxidation"],
        usage=["wire", "frame"],
        next_recipe_candidates=["reinforced_wire"],
        visual_prompt="iron copper alloy ingot",
    )

    hash1 = generate_material_hash(proposal1)
    hash2 = generate_material_hash(proposal2)

    assert hash1 == hash2
