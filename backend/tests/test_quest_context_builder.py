"""Unit tests for game_data and QuestContextBuilder."""

from __future__ import annotations

from agents.quest_generator import game_data
from agents.quest_generator.context_builder import QuestContextBuilder


def test_game_data_loader() -> None:
    # 1. Test resources loading
    valid_ids = game_data.get_valid_item_ids()
    assert "resource_wood" in valid_ids
    assert "resource_iron_ingot" in valid_ids
    assert "resource_copper_ingot" in valid_ids

    # 2. Test item name mapping
    assert game_data.get_item_name("resource_wood") == "목재"
    assert game_data.get_item_name("resource_iron_ingot") == "철괴"
    assert game_data.get_item_name("resource_copper_ingot") == "구리괴"
    # Fallback case
    assert game_data.get_item_name("resource_unknown_item") == "resource_unknown_item"

    # 3. Test recipes loading
    recipe_map = game_data.get_recipe_map()
    assert "recipe_smelt_iron" in recipe_map
    out_item, in_items = recipe_map["recipe_smelt_iron"]
    assert out_item == "resource_iron_ingot"
    assert "resource_iron_ore" in in_items


def test_quest_context_builder_normalization() -> None:
    assert QuestContextBuilder.normalize_item_id("iron_ingot") == "resource_iron_ingot"
    assert QuestContextBuilder.normalize_item_id("resource_iron_ingot") == "resource_iron_ingot"
    assert QuestContextBuilder.normalize_item_id("") == ""

    assert QuestContextBuilder.normalize_recipe_id("smelt_iron") == "recipe_smelt_iron"
    assert QuestContextBuilder.normalize_recipe_id("recipe_smelt_iron") == "recipe_smelt_iron"
    assert QuestContextBuilder.normalize_recipe_id("") == ""


def test_quest_context_builder_build_context_success() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "current_main_quest": {
            "quest_id": "main_commtower",
            "title": "통신탑 건설",
            "objectives": [
                {
                    "main_objective_id": "need_iron_ingot",
                    "objective_type": "collect_item",
                    "item_id": "iron_ingot",  # needs normalization to resource_iron_ingot
                    "required": 20,
                    "current": 8,
                },
                {
                    "main_objective_id": "need_copper_ingot",
                    "objective_type": "collect_item",
                    "item_id": "resource_copper_ingot",  # already normalized
                    "required": 10,
                    "current": 2,
                },
            ],
        },
        "inventory": {
            "iron_ore": 120,
            "iron_ingot": 8,
            "copper_ore": 80,
            "resource_copper_ingot": 2,
        },
        "recent_production": {
            "iron_ingot": 4,
            "copper_ingot": 1,
        },
        "unlocked_machines": ["Miner_Lv1", "Smelter_Lv1", "Assembler_Lv1"],
        "unlocked_recipes": [
            "smelt_iron",  # needs normalization to recipe_smelt_iron
            "smelt_copper",  # needs normalization to recipe_smelt_copper
        ],
        "active_support_quest_ids": [],
        "completed_quest_ids": ["tutorial_001", "tutorial_002", "main_001"],
    }

    context = QuestContextBuilder.build_context(payload)

    # Verify basic mapping & normalization
    assert context.factory_id == "factory_001"
    assert context.factory_level == 2
    assert "resource_iron_ingot" in context.inventory
    assert context.inventory["resource_iron_ingot"] == 8
    assert "recipe_smelt_iron" in context.unlocked_recipes

    # Verify main quest parsing
    assert context.current_main_quest is not None
    assert context.current_main_quest.quest_id == "main_commtower"
    assert len(context.current_main_quest.objectives) == 2
    assert context.current_main_quest.objectives[0].item_id == "resource_iron_ingot"

    # Verify known_issues calculation
    # shortage_amount: iron_ingot (20 - 8 = 12), copper_ingot (10 - 2 = 8)
    assert len(context.known_issues) == 2

    iron_issue = next(issue for issue in context.known_issues if issue.item_id == "resource_iron_ingot")
    assert iron_issue.shortage_amount == 12
    assert iron_issue.main_objective_id == "need_iron_ingot"
    # producible: smelt_iron is in unlocked_recipes
    assert iron_issue.producible is True

    copper_issue = next(issue for issue in context.known_issues if issue.item_id == "resource_copper_ingot")
    assert copper_issue.shortage_amount == 8
    assert copper_issue.main_objective_id == "need_copper_ingot"
    # producible: smelt_copper is in unlocked_recipes
    assert copper_issue.producible is True


def test_quest_context_builder_build_context_not_producible() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "current_main_quest": {
            "quest_id": "main_commtower",
            "title": "통신탑 건설",
            "objectives": [
                {
                    "main_objective_id": "need_copper_ingot",
                    "objective_type": "collect_item",
                    "item_id": "copper_ingot",
                    "required": 10,
                    "current": 2,
                }
            ],
        },
        "inventory": {},
        "unlocked_recipes": [
            "smelt_iron"  # recipe_smelt_copper is not unlocked
        ],
    }

    context = QuestContextBuilder.build_context(payload)

    assert len(context.known_issues) == 1
    copper_issue = context.known_issues[0]
    assert copper_issue.item_id == "resource_copper_ingot"
    assert copper_issue.shortage_amount == 8
    # producible should be False since smelt_copper is not in unlocked_recipes
    assert copper_issue.producible is False


def test_db_model_to_pydantic_conversion() -> None:
    from datetime import UTC, datetime

    from db.models import QuestInstanceModel, QuestProgressModel

    # 1. Test QuestInstanceModel.to_pydantic
    db_instance = QuestInstanceModel(
        id="qinst_001",
        factory_id="factory_001",
        quest_type="support",
        support_type="collect_item",
        related_main_quest_id="main_001",
        title="철괴 확보",
        description="철괴를 확보하세요.",
        status="in_progress",
        objective_json=[{
            "id": "obj_001",
            "type": "collect_item",
            "target_id": "resource_iron_ingot",
            "target_amount": 10,
            "current_amount": 2,
            "status": "in_progress",
        }],
        reward_json=[{
            "type": "currency",
            "target_id": "gold",
            "amount": 100,
        }],
        created_by="rule",
        level_reward_applied=False,
        created_at=datetime.now(UTC),
        completed_at=None,
    )
    pydantic_instance = db_instance.to_pydantic()
    assert pydantic_instance.id == "qinst_001"
    assert len(pydantic_instance.objectives) == 1
    assert pydantic_instance.objectives[0].target_id == "resource_iron_ingot"
    assert len(pydantic_instance.rewards) == 1
    assert pydantic_instance.rewards[0].amount == 100

    # 2. Test QuestProgressModel.to_pydantic
    db_progress = QuestProgressModel(
        id="qprog_001",
        quest_instance_id="qinst_001",
        objective_id="obj_001",
        objective_type="collect_item",
        target_id="resource_iron_ingot",
        current_amount=2,
        target_amount=10,
        status="in_progress",
        last_event_id="evt_001",
    )
    pydantic_progress = db_progress.to_pydantic()
    assert pydantic_progress.id == "qprog_001"
    assert pydantic_progress.target_amount == 10
    assert pydantic_progress.last_event_id == "evt_001"
