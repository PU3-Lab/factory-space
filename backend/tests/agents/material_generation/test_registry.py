"""동시성 경쟁 상태(Concurrency race conditions)에 초점을 맞춘 ExperimentRegistryService 단위 테스트입니다."""

from __future__ import annotations

import uuid

from sqlalchemy.orm import Session

from agents.material_generation.registry.experiment_registry import (
    ExperimentRegistryService,
)
from db.models import GeneratedExperimentModel


def test_save_experiment_concurrency_race(db_session: Session) -> None:
    """동일한 해시를 가지지만 ID가 다른 두 실험을 저장할 때 충돌이 정상적으로 처리되는지 테스트합니다."""
    exp_hash = "test_item_1:2|test_item_2:3"

    # 1. 첫 번째 실험
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

    # 2. 동시성 경쟁: 동일한 해시를 가지지만 ID가 다르고 속성이 업데이트된 다른 실험 저장
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
        result_type="new_material",  # 다른 result_type
        material_id="new_mat_123",
        failure_reason="overwritten_run",
    )

    # 이는 IntegrityError를 발생시키지 않고 문제없이 병합(Merge)되어야 함
    ExperimentRegistryService.save_experiment(db_session, exp2)
    db_session.commit()

    # 3. 레코드가 존재하고 올바르게 병합되었는지 조회 및 검증
    retrieved = ExperimentRegistryService.get_experiment_by_hash(db_session, exp_hash)
    assert retrieved is not None
    assert retrieved.id == exp1.id  # 원래 ID를 유지해야 함
    assert retrieved.result_type == "new_material"  # 병합/업데이트되었어야 함
    assert retrieved.material_id == "new_mat_123"
