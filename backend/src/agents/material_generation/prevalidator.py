"""Deterministic validation of inputs before running optimization/synthesis."""

from __future__ import annotations

from typing import Any

from sqlalchemy.orm import Session

from agents.material_generation.recipe_repository import RecipeRepository

VALID_MACHINES = {"Smelter", "Grinder", "Synthesizer"}


class RecipePreValidator:
    """Performs static checks on materials, quantities, and machine types."""

    @classmethod
    def validate_inputs(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
    ) -> str | None:
        """Validate input parameters. Returns error code if invalid, else None."""
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
