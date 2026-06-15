"""최적화/합성을 실행하기 전 입력값에 대한 결정론적 검증입니다."""

from __future__ import annotations

from typing import Any

from sqlalchemy.orm import Session

from agents.material_generation.recipe_repository import RecipeRepository

VALID_MACHINES = {"Smelter", "Grinder", "Synthesizer"}


class RecipePreValidator:
    """재료, 수량 및 머신 유형에 대해 정적 검사를 수행합니다."""

    @classmethod
    def validate_inputs(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
    ) -> str | None:
        """입력 파라미터를 검증합니다. 유효하지 않은 경우 에러 코드를 반환하고, 유효한 경우 None을 반환합니다."""
        if machine_type not in VALID_MACHINES:
            return "invalid_machine"

        if not normalized_inputs:
            return "empty_inputs"

        known_items = RecipeRepository.get_known_items(session)

        for item in normalized_inputs:
            item_id = item["item_id"]
            qty = item["qty"]

            if qty <= 0:
                return "invalid_qty"

            if item_id not in known_items:
                return "unknown_item"

        return None
