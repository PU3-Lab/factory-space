"""Retrieves previously discovered similar experiments to assist LLM generation."""

from __future__ import annotations

from typing import Any

from sqlalchemy import String, cast, or_, select
from sqlalchemy.orm import Session

from db.models import GeneratedExperimentModel, GeneratedMaterialModel


class ExperimentSimilarityService:
    """Provides historical context for synthesis experiments based on ingredient overlap."""

    @classmethod
    def find_similar_experiments(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
        limit: int = 3,
    ) -> list[dict[str, Any]]:
        """Find past successful experiments with overlapping inputs on the same machine."""
        input_item_ids = {item["item_id"] for item in normalized_inputs}
        if not input_item_ids:
            return []

        # Optimize: Filter on the database side using LIKE queries to avoid a full table scan
        clauses = [
            cast(GeneratedExperimentModel.inputs_json, String).like(f'%"{item_id}"%')
            for item_id in input_item_ids
        ]

        from sqlalchemy import case

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

        # Query past experiments of the same machine type that successfully created a material
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
            # inputs_json: list of dicts like [{"item_id": "...", "qty": ...}]
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

        # Sort by overlap count (descending)
        similar_candidates.sort(key=lambda x: x["overlap_count"], reverse=True)

        # Remove internal sorting keys before returning
        return [
            {
                "inputs": item["inputs"],
                "material_name": item["material_name"],
                "material_category": item["material_category"],
                "properties": item["properties"],
            }
            for item in similar_candidates[:limit]
        ]
