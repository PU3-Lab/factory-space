"""입력 정규화기(Normalizer) 및 해싱 함수에 대한 단위 테스트입니다."""

from __future__ import annotations

from agents.material_generation.normalizer import (
    generate_experiment_hash,
    generate_material_hash,
    normalize_inputs,
)
from agents.material_generation.schemas import (
    InputItemSchema,
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


def test_material_hash_is_synthesis_identity() -> None:
    """합성 장비, 정규화된 입력 재료, 공정 조건이 동일할 때 동일한 해시가 생성되는지 테스트합니다."""
    from agents.material_generation.schemas import ProcessConditionsSchema

    inputs = [
        {"item_id": "copper_ingot", "qty": 1},
        {"item_id": "iron_ingot", "qty": 2},
    ]
    h1 = generate_material_hash("Synthesizer", inputs, ProcessConditionsSchema())
    h2 = generate_material_hash("Synthesizer", inputs, ProcessConditionsSchema())
    assert h1 == h2
    assert len(h1) == 64


def test_material_hash_differs_on_inputs() -> None:
    """입력이 다를 경우 해시값이 다르게 생성되는지 테스트합니다."""
    from agents.material_generation.schemas import ProcessConditionsSchema

    a = generate_material_hash(
        "Synthesizer", [{"item_id": "iron_ingot", "qty": 1}], ProcessConditionsSchema()
    )
    b = generate_material_hash(
        "Synthesizer",
        [{"item_id": "copper_ingot", "qty": 1}],
        ProcessConditionsSchema(),
    )
    assert a != b
