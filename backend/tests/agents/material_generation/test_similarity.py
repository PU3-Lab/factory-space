"""Unit tests for the ExperimentSimilarityService, verifying SQL-side overlap calculation and limit."""

from __future__ import annotations

from sqlalchemy.orm import Session

from agents.material_generation.similarity import ExperimentSimilarityService
from db.models import GeneratedExperimentModel, GeneratedMaterialModel


def test_find_similar_experiments_ordering_and_limit(db_session: Session) -> None:
    """Test that find_similar_experiments correctly orders by overlap count and respects limits."""
    # 1. Insert sample materials and experiments
    # Material 1
    mat1 = GeneratedMaterialModel(
        id="mat_1",
        material_hash="hash_mat_1",
        name="Super Iron Alloy",
        category="metal",
        rarity="rare",
        properties_json={"hardness": 80},
    )
    db_session.add(mat1)

    # Material 2
    mat2 = GeneratedMaterialModel(
        id="mat_2",
        material_hash="hash_mat_2",
        name="Copper Compound",
        category="metal",
        rarity="common",
        properties_json={"conductivity": 95},
    )
    db_session.add(mat2)
    db_session.commit()

    # Experiment 1: inputs_json has 'iron' (overlap 1)
    exp1 = GeneratedExperimentModel(
        id="exp_1",
        experiment_hash="hash_exp_1",
        machine_type="Smelter",
        inputs_json=[{"item_id": "iron", "qty": 2}],
        normalized_inputs_json=[{"item_id": "iron", "qty": 2}],
        classification="simple_variation",
        result_type="new_material",
        material_id="mat_1",
    )
    db_session.add(exp1)

    # Experiment 2: inputs_json has 'iron' and 'copper' (overlap 2)
    exp2 = GeneratedExperimentModel(
        id="exp_2",
        experiment_hash="hash_exp_2",
        machine_type="Smelter",
        inputs_json=[{"item_id": "iron", "qty": 2}, {"item_id": "copper", "qty": 1}],
        normalized_inputs_json=[
            {"item_id": "iron", "qty": 2},
            {"item_id": "copper", "qty": 1},
        ],
        classification="simple_variation",
        result_type="new_material",
        material_id="mat_2",
    )
    db_session.add(exp2)
    db_session.commit()

    # 2. Query for similar experiments with inputs containing 'iron' and 'copper'
    inputs = [{"item_id": "iron", "qty": 2}, {"item_id": "copper", "qty": 1}]

    # Check that order is correct (exp2 has overlap 2, exp1 has overlap 1)
    results = ExperimentSimilarityService.find_similar_experiments(
        session=db_session, machine_type="Smelter", normalized_inputs=inputs, limit=2
    )

    assert len(results) == 2
    # The first result must be 'Copper Compound' (mat_2) because it overlaps on both iron and copper
    assert results[0]["material_name"] == "Copper Compound"
    assert results[1]["material_name"] == "Super Iron Alloy"

    # Test limit constraint
    limited_results = ExperimentSimilarityService.find_similar_experiments(
        session=db_session, machine_type="Smelter", normalized_inputs=inputs, limit=1
    )
    assert len(limited_results) == 1
    assert limited_results[0]["material_name"] == "Copper Compound"
