from __future__ import annotations

import unittest.mock

from agents.quest_generator.context_builder import QuestContextBuilder
from agents.quest_generator.models import QuestObjective, QuestReward, SupportQuestDraft
from agents.quest_generator.validator import QuestValidator


def test_validate_success_inventory() -> None:
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
                    "item_id": "iron_ingot",
                    "required": 20,
                    "current": 8,
                }
            ],
        },
        "inventory": {
            "iron_ingot": 8,
        },
    }
    context = QuestContextBuilder.build_context(payload)

    draft = SupportQuestDraft(
        title="철괴 확보 지원",
        description="철괴 확보를 지원합니다.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_001",
                type="collect_item",
                target_id="resource_iron_ingot",
                target_amount=20,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    result = QuestValidator.validate(draft, context, set())
    assert result.valid is True
    assert result.reason is None


def test_validate_success_raw_material() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "inventory": {},
    }
    context = QuestContextBuilder.build_context(payload)

    draft = SupportQuestDraft(
        title="목재 확보 지원",
        description="목재를 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_002",
                type="collect_item",
                target_id="resource_wood",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    result = QuestValidator.validate(draft, context, set())
    assert result.valid is True


def test_validate_success_producible_inputs() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "inventory": {
            "iron_ore": 5,
        },
        "unlocked_recipes": ["smelt_iron"],
    }
    context = QuestContextBuilder.build_context(payload)

    draft = SupportQuestDraft(
        title="철괴 확보 지원",
        description="철괴 확보를 지원합니다.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_003",
                type="collect_item",
                target_id="resource_iron_ingot",
                target_amount=20,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    result = QuestValidator.validate(draft, context, set())
    assert result.valid is True


def test_validate_fail_duplicate() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "inventory": {"iron_ingot": 5},
    }
    context = QuestContextBuilder.build_context(payload)

    draft = SupportQuestDraft(
        title="철괴 확보 지원",
        description="철괴 확보를 지원합니다.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_004",
                type="collect_item",
                target_id="resource_iron_ingot",
                target_amount=20,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    result = QuestValidator.validate(draft, context, {"resource_iron_ingot"})
    assert result.valid is False
    assert result.reason == "duplicate_support_quest"
    assert "활성화" in result.message


def test_validate_fail_invalid_item() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "inventory": {},
    }
    context = QuestContextBuilder.build_context(payload)

    draft = SupportQuestDraft(
        title="가짜 아이템 확보 지원",
        description="가짜 아이템을 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_005",
                type="collect_item",
                target_id="resource_fake_item",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    result = QuestValidator.validate(draft, context, set())
    assert result.valid is False
    assert result.reason == "invalid_item"


def test_validate_fail_invalid_amount() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "inventory": {"iron_ingot": 5},
    }
    context = QuestContextBuilder.build_context(payload)

    draft = SupportQuestDraft(
        title="철괴 확보 지원",
        description="철괴 확보를 지원합니다.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_006",
                type="collect_item",
                target_id="resource_iron_ingot",
                target_amount=5000,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    result = QuestValidator.validate(draft, context, set())
    assert result.valid is False
    assert result.reason == "invalid_amount"


def test_validate_fail_unreachable_prerequisite() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "inventory": {},
        "unlocked_recipes": [],
    }
    context = QuestContextBuilder.build_context(payload)

    draft = SupportQuestDraft(
        title="철괴 확보 지원",
        description="철괴 확보를 지원합니다.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_007",
                type="collect_item",
                target_id="resource_iron_ingot",
                target_amount=20,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    result = QuestValidator.validate(draft, context, set())
    assert result.valid is False
    assert result.reason == "unreachable_prerequisite"
    assert "선행조건" in result.message or "레시피" in result.message


def test_validate_success_multilevel_producible() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 3,
        "inventory": {
            "iron_ore": 10,
        },
        "unlocked_recipes": ["smelt_steel", "make_steel_plate", "smelt_iron"],
    }
    context = QuestContextBuilder.build_context(payload)

    mock_recipe_map = {
        "recipe_make_steel_plate": ("resource_steel_plate", ["resource_steel_ingot"]),
        "recipe_smelt_steel": ("resource_steel_ingot", ["resource_iron_ore"]),
    }

    draft = SupportQuestDraft(
        title="강판 확보 지원",
        description="강판 확보를 지원합니다.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_008",
                type="collect_item",
                target_id="resource_steel_plate",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    with unittest.mock.patch(
        "agents.quest_generator.game_data.get_recipe_map", return_value=mock_recipe_map
    ):
        with unittest.mock.patch(
            "agents.quest_generator.game_data.get_valid_item_ids",
            return_value={
                "resource_steel_plate",
                "resource_steel_ingot",
                "resource_iron_ore",
            },
        ):
            result = QuestValidator.validate(draft, context, set())
            assert result.valid is True


def test_validate_fail_multilevel_unreachable() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 3,
        "inventory": {
            "iron_ore": 10,
        },
        "unlocked_recipes": ["make_steel_plate"],
    }
    context = QuestContextBuilder.build_context(payload)

    mock_recipe_map = {
        "recipe_make_steel_plate": ("resource_steel_plate", ["resource_steel_ingot"]),
        "recipe_smelt_steel": ("resource_steel_ingot", ["resource_iron_ore"]),
    }

    draft = SupportQuestDraft(
        title="강판 확보 지원",
        description="강판 확보를 지원합니다.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_009",
                type="collect_item",
                target_id="resource_steel_plate",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    with unittest.mock.patch(
        "agents.quest_generator.game_data.get_recipe_map", return_value=mock_recipe_map
    ):
        with unittest.mock.patch(
            "agents.quest_generator.game_data.get_valid_item_ids",
            return_value={
                "resource_steel_plate",
                "resource_steel_ingot",
                "resource_iron_ore",
            },
        ):
            result = QuestValidator.validate(draft, context, set())
            assert result.valid is False
            assert result.reason == "unreachable_prerequisite"


def test_validate_fail_circular_dependency() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 3,
        "inventory": {},
        "unlocked_recipes": ["recipe_a", "recipe_b"],
    }
    context = QuestContextBuilder.build_context(payload)

    mock_recipe_map = {
        "recipe_a": ("resource_item_a", ["resource_item_b"]),
        "recipe_b": ("resource_item_b", ["resource_item_a"]),
    }

    draft = SupportQuestDraft(
        title="아이템 A 확보 지원",
        description="아이템 A를 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_010",
                type="collect_item",
                target_id="resource_item_a",
                target_amount=5,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    with unittest.mock.patch(
        "agents.quest_generator.game_data.get_recipe_map", return_value=mock_recipe_map
    ):
        with unittest.mock.patch(
            "agents.quest_generator.game_data.get_valid_item_ids",
            return_value={"resource_item_a", "resource_item_b"},
        ):
            result = QuestValidator.validate(draft, context, set())
            assert result.valid is False
            assert result.reason == "unreachable_prerequisite"
