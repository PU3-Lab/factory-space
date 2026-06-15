"""Unit tests for the ExperimentRegistryService, focusing on concurrency race conditions."""

from __future__ import annotations

import uuid

from sqlalchemy.orm import Session

from agents.material_generation.registry.experiment_registry import (
    ExperimentRegistryService,
)
from db.models import GeneratedExperimentModel


def test_save_experiment_concurrency_race(db_session: Session) -> None:
    """Test that saving two experiments with the same hash but different IDs handles the collision gracefully."""
    exp_hash = "test_item_1:2|test_item_2:3"

    # 1. First experiment
    exp1 = GeneratedExperimentModel(
        id=str(uuid.uuid4()),
        experiment_hash=exp_hash,
        machine_type="Smelter",
        inputs_json=[
            {"item_id": "test_item_1", "qty": 2},
            {"item_id": "test_item_2", "qty": 3},
        ],
        normalized_inputs_json=[
            {"item_id": "test_item_1", "qty": 2},
            {"item_id": "test_item_2", "qty": 3},
        ],
        classification="simple_variation",
        result_type="failure",
        failure_reason="initial_run",
    )

    ExperimentRegistryService.save_experiment(db_session, exp1)
    db_session.commit()

    # 2. Concurrency race: save another experiment with the same hash but different ID and updated attributes
    exp2 = GeneratedExperimentModel(
        id=str(uuid.uuid4()),
        experiment_hash=exp_hash,
        machine_type="Smelter",
        inputs_json=[
            {"item_id": "test_item_1", "qty": 2},
            {"item_id": "test_item_2", "qty": 3},
        ],
        normalized_inputs_json=[
            {"item_id": "test_item_1", "qty": 2},
            {"item_id": "test_item_2", "qty": 3},
        ],
        classification="simple_variation",
        result_type="new_material",  # different result_type
        material_id="new_mat_123",
        failure_reason="overwritten_run",
    )

    # This should merge gracefully instead of raising IntegrityError
    ExperimentRegistryService.save_experiment(db_session, exp2)
    db_session.commit()

    # 3. Retrieve and assert that the record exists and has been merged correctly
    retrieved = ExperimentRegistryService.get_experiment_by_hash(db_session, exp_hash)
    assert retrieved is not None
    assert retrieved.id == exp1.id  # should keep the original ID
    assert retrieved.result_type == "new_material"  # should have been merged/updated
    assert retrieved.material_id == "new_mat_123"
