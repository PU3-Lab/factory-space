"""QuestContextBuilder for constructing QuestContext from client snapshot payloads."""

from __future__ import annotations

from agents.quest_generator import game_data
from agents.quest_generator.models import (
    CurrentMainQuest,
    KnownIssue,
    MainQuestObjective,
    QuestContext,
)


class QuestContextBuilder:
    """Builder class that parses and normalizes the client snapshot into a structured QuestContext."""

    @staticmethod
    def normalize_item_id(item_id: str) -> str:
        """Normalizes plain item IDs to the single-source-of-truth resource_* namespace."""
        if not item_id:
            return ""
        if not item_id.startswith("resource_"):
            return f"resource_{item_id}"
        return item_id

    @staticmethod
    def normalize_recipe_id(recipe_id: str) -> str:
        """Normalizes plain recipe IDs to the recipe_* namespace."""
        if not recipe_id:
            return ""
        if not recipe_id.startswith("recipe_"):
            return f"recipe_{recipe_id}"
        return recipe_id

    @classmethod
    def build_context(cls, payload: dict) -> QuestContext:
        """Constructs a normalized QuestContext Pydantic model from a raw payload dict."""
        factory_id = payload.get("factory_id", "")
        factory_level = payload.get("factory_level", 1)

        # Normalize inventory keys
        raw_inventory = payload.get("inventory", {})
        inventory = {cls.normalize_item_id(k): v for k, v in raw_inventory.items()}

        # Normalize recent production keys
        raw_production = payload.get("recent_production", {})
        recent_production = {cls.normalize_item_id(k): v for k, v in raw_production.items()}

        # Normalize lists
        unlocked_machines = payload.get("unlocked_machines", [])
        raw_recipes = payload.get("unlocked_recipes", [])
        unlocked_recipes = [cls.normalize_recipe_id(r) for r in raw_recipes]

        active_support_quest_ids = payload.get("active_support_quest_ids", [])
        completed_quest_ids = payload.get("completed_quest_ids", [])

        # Parse and normalize current main quest
        current_main_quest_data = payload.get("current_main_quest")
        current_main_quest = None
        known_issues = []

        if current_main_quest_data:
            quest_id = current_main_quest_data.get("quest_id", "")
            title = current_main_quest_data.get("title", "")
            objectives_data = current_main_quest_data.get("objectives", [])

            objectives = []
            recipe_map = game_data.get_recipe_map()

            for obj in objectives_data:
                obj_id = obj.get("main_objective_id", "")
                obj_type = obj.get("objective_type", "")
                item_id = cls.normalize_item_id(obj.get("item_id", ""))
                objective_model = MainQuestObjective(
                    main_objective_id=obj_id,
                    objective_type=obj_type,
                    item_id=item_id,
                    required=obj.get("required", 0),
                    current=obj.get("current", 0),
                )
                objectives.append(objective_model)

                # Generate KnownIssue if the main quest objective is not fully met
                if objective_model.required > objective_model.current:
                    shortage_amount = objective_model.required - objective_model.current

                    # Check if the shortage item is producible (unlocked recipe outputs it)
                    producible = False
                    for r_id, (out_item, _) in recipe_map.items():
                        norm_r_id = cls.normalize_recipe_id(r_id)
                        if out_item == item_id and norm_r_id in unlocked_recipes:
                            producible = True
                            break

                    known_issues.append(
                        KnownIssue(
                            item_id=item_id,
                            shortage_amount=shortage_amount,
                            main_objective_id=obj_id,
                            producible=producible,
                        )
                    )

            current_main_quest = CurrentMainQuest(
                quest_id=quest_id,
                title=title,
                objectives=objectives,
            )

        return QuestContext(
            factory_id=factory_id,
            factory_level=factory_level,
            current_main_quest=current_main_quest,
            inventory=inventory,
            recent_production=recent_production,
            unlocked_machines=unlocked_machines,
            unlocked_recipes=unlocked_recipes,
            active_support_quest_ids=active_support_quest_ids,
            completed_quest_ids=completed_quest_ids,
            known_issues=known_issues,
        )
