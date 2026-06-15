"""LLM 생성을 지원하기 위해 이전에 발견된 유사한 실험 정보를 검색합니다."""

from __future__ import annotations

from typing import Any

from sqlalchemy import String, case, cast, or_, select
from sqlalchemy.orm import Session

from db.models import GeneratedExperimentModel, GeneratedMaterialModel


class ExperimentSimilarityService:
    """재료의 중복 여부를 기반으로 합성 실험에 대한 이력 컨텍스트를 제공합니다."""

    @classmethod
    def find_similar_experiments(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
        limit: int = 3,
    ) -> list[dict[str, Any]]:
        """동일한 장비에서 입력 재료가 중복되는 과거 성공 실험을 찾습니다."""
        input_item_ids = {item["item_id"] for item in normalized_inputs}
        if not input_item_ids:
            return []

        # 최적화: 전체 테이블 스캔을 방지하기 위해 LIKE 쿼리를 사용하여 데이터베이스 측에서 필터링합니다.
        clauses = [
            cast(GeneratedExperimentModel.inputs_json, String).like(f'%"{item_id}"%')
            for item_id in input_item_ids
        ]

        overlap_score = sum(
            case(
                (
                    cast(GeneratedExperimentModel.inputs_json, String).like(
                        f'%"{item_id}"%'
                    ),
                    1,
                ),
                else_=0,
            )
            for item_id in input_item_ids
        )

        # 새로운 재료를 성공적으로 생성한 동일한 장비 유형의 과거 실험을 쿼리합니다.
        stmt = (
            select(GeneratedExperimentModel, GeneratedMaterialModel)
            .join(
                GeneratedMaterialModel,
                GeneratedExperimentModel.material_id == GeneratedMaterialModel.id,
            )
            .where(
                GeneratedExperimentModel.machine_type == machine_type,
                GeneratedExperimentModel.result_type == "new_material",
                or_(*clauses),
            )
            .order_by(overlap_score.desc())
            .limit(limit * 3)
        )
        result = session.execute(stmt)
        rows = result.all()

        similar_candidates = []
        for exp_model, mat_model in rows:
            # inputs_json: [{"item_id": "...", "qty": ...}] 형식의 딕셔너리 리스트
            exp_inputs = exp_model.inputs_json
            if not isinstance(exp_inputs, list):
                continue

            exp_item_ids = {item["item_id"] for item in exp_inputs if "item_id" in item}
            overlap = input_item_ids.intersection(exp_item_ids)

            if overlap:
                similar_candidates.append(
                    {
                        "overlap_count": len(overlap),
                        "experiment_id": exp_model.id,
                        "inputs": exp_inputs,
                        "material_name": mat_model.name,
                        "material_category": mat_model.category,
                        "properties": mat_model.properties_json,
                    }
                )

        # 중복 카운트 기준 정렬 (내림차순)
        similar_candidates.sort(key=lambda x: x["overlap_count"], reverse=True)

        # 반환하기 전에 내부 정렬 키를 제거합니다.
        return [
            {
                "inputs": item["inputs"],
                "material_name": item["material_name"],
                "material_category": item["material_category"],
                "properties": item["properties"],
            }
            for item in similar_candidates[:limit]
        ]
