"""Rules engine to classify unknown experiments before LLM intervention."""

from __future__ import annotations

from typing import Any

from sqlalchemy.orm import Session

from agents.material_generation.recipe_repository import RecipeRepository


class ExperimentClassifier:
    """Classifies invalid/non-recipe combinations into deterministic outcomes or LLM candidates."""

    @classmethod
    def classify(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
    ) -> str:
        """Classify the experiment setup.

        Returns one of: 'simple_variation', 'intermediate_material', 'failed_result',
        'candidate', 'ambiguous'.
        """
        # Ensure recipes cache is loaded
        recipes = RecipeRepository.get_recipes(session)

        input_items = {item["item_id"] for item in normalized_inputs}

        # 1. Check for simple_variation:
        # Same machine type and same set of ingredients, but quantities differ
        for r in recipes:
            if r.machine_type != machine_type:
                continue

            r_items = set()
            if r.input_item_1:
                r_items.add(r.input_item_1)
            if r.input_item_2:
                r_items.add(r.input_item_2)
            if r.input_item_3:
                r_items.add(r.input_item_3)

            if r_items == input_items:
                return "simple_variation"

        # 2. Check for failed_result:
        # Mixing 2+ items in Smelter is invalid (Smelter is basic thermal processing, not synthesis)
        if len(input_items) >= 2 and machine_type == "Smelter":
            return "failed_result"

        # 3. Check for intermediate_material:
        # Grinding 2+ ingredients together makes a mixed powder (intermediate)
        if len(input_items) >= 2 and machine_type == "Grinder":
            return "intermediate_material"

        # 4. Check for candidate:
        # Synthesizer with 2+ items is a prime candidate for a new alloy or compound
        if len(input_items) >= 2 and machine_type == "Synthesizer":
            return "candidate"

        # Default fallback for single-ingredient or other obscure edge cases
        return "ambiguous"
