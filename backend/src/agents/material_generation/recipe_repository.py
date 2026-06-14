"""Repository for querying and matching system recipes from the database."""

from __future__ import annotations

from typing import Any

from sqlalchemy import select
from sqlalchemy.orm import Session

from db.models import RecipeModel


class RecipeRepository:
    """Manages cached known items and handles matching of authored recipes."""

    @classmethod
    def reload_cache(cls, session: Session) -> None:
        """Load all recipes from DB and populate session memory cache."""
        result = session.execute(select(RecipeModel))
        recipes = list(result.scalars().all())
        session.info["recipes_cache"] = recipes

        items = set()
        for r in recipes:
            for item in (
                r.input_item_1,
                r.input_item_2,
                r.input_item_3,
                r.output_item_1,
                r.output_item_2,
            ):
                if item:
                    items.add(item)
        session.info["known_items"] = items

    @classmethod
    def get_known_items(cls, session: Session) -> set[str]:
        """Return the set of all item IDs present in existing recipes."""
        if "known_items" not in session.info:
            cls.reload_cache(session)
        return session.info["known_items"]

    @classmethod
    def get_recipes(cls, session: Session) -> list[RecipeModel]:
        """Return the list of all recipes."""
        if "recipes_cache" not in session.info:
            cls.reload_cache(session)
        return session.info["recipes_cache"]

    @classmethod
    def match_recipe(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
    ) -> RecipeModel | None:
        """Find a recipe completely matching the given machine and input items/quantities."""
        recipes = cls.get_recipes(session)
        input_map = {item["item_id"]: item["qty"] for item in normalized_inputs}

        for r in recipes:
            if r.machine_type != machine_type:
                continue

            recipe_inputs = {}
            if r.input_item_1:
                recipe_inputs[r.input_item_1] = r.input_qty_1
            if r.input_item_2:
                recipe_inputs[r.input_item_2] = r.input_qty_2
            if r.input_item_3:
                recipe_inputs[r.input_item_3] = r.input_qty_3

            if recipe_inputs == input_map:
                return r

        return None
