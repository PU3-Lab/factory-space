"""SQL 기반의 중복도 계산 및 제한(limit) 검증을 위한 ExperimentSimilarityService 단위 테스트입니다."""

from __future__ import annotations

from sqlalchemy.orm import Session

from agents.material_generation.similarity import ExperimentSimilarityService
from db.models import GeneratedExperimentModel, GeneratedMaterialModel


def test_find_similar_experiments_ordering_and_limit(db_session: Session) -> None:
    """find_similar_experiments가 중복 카운트 기준 정렬을 올바르게 수행하고 제한(limit)을 준수하는지 테스트합니다."""
    # 1. 샘플 재료 및 실험 데이터 삽입
    # 재료 1
    mat1 = GeneratedMaterialModel(
        id="mat_1",
        material_hash="hash_mat_1",
        name="Super Iron Alloy",
        category="metal",
        rarity="rare",
        properties_json={"hardness": 80},
    )
    db_session.add(mat1)

    # 재료 2
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

    # 실험 1: inputs_json에 'iron' 포함 (중복 1)
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

    # 실험 2: inputs_json에 'iron' 및 'copper' 포함 (중복 2)
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

    # 2. 'iron' 및 'copper'가 포함된 입력을 대입하여 유사 실험 쿼리
    inputs = [{"item_id": "iron", "qty": 2}, {"item_id": "copper", "qty": 1}]

    # 정렬 순서가 올바른지 확인 (exp2는 중복 2, exp1은 중복 1)
    results = ExperimentSimilarityService.find_similar_experiments(
        session=db_session, machine_type="Smelter", normalized_inputs=inputs, limit=2
    )

    assert len(results) == 2
    # 첫 번째 결과는 iron과 copper 둘 다 중복되는 'Copper Compound' (mat_2)여야 함
    assert results[0]["material_name"] == "Copper Compound"
    assert results[1]["material_name"] == "Super Iron Alloy"

    # limit 제약 조건 테스트
    limited_results = ExperimentSimilarityService.find_similar_experiments(
        session=db_session, machine_type="Smelter", normalized_inputs=inputs, limit=1
    )
    assert len(limited_results) == 1
    assert limited_results[0]["material_name"] == "Copper Compound"
