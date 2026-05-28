from __future__ import annotations

import json

from factory_space.agents.quest.schemas import QuestGenerationResult
from factory_space.agents.quest.service import QuestAgentService


def test_generates_control_panel_inspection_quest_json() -> None:
    service = QuestAgentService()

    result_json = service.generate_quest_json(
        {
            "event": "player_entered_area",
            "game_state": {
                "player_location": "machine_room",
                "nearby_objects": [
                    {
                        "id": "control_panel_01",
                        "type": "control_panel",
                        "status": "idle",
                    }
                ],
            },
        }
    )

    result = QuestGenerationResult.model_validate(json.loads(result_json))

    assert result.quest.quest_id == "quest-inspect-control_panel_01"
    assert result.quest.title == "Inspect the control panel"
    assert result.quest.objective.target_object_id == "control_panel_01"
    assert result.quest.objective.required_event == "player_interacted"
    assert result.quest.priority == "medium"
    assert [action.name for action in result.actions] == [
        "show_ui_message",
        "highlight_object",
        "update_quest_marker",
    ]


def test_generates_iron_ore_mining_quest_json() -> None:
    service = QuestAgentService()

    result_json = service.generate_quest_json(
        {
            "event": "player_entered_area",
            "game_state": {
                "player_location": "mine_zone",
                "nearby_objects": [
                    {
                        "id": "iron_ore_node_01",
                        "type": "resource_node",
                        "status": "available",
                    }
                ],
            },
        }
    )

    result = QuestGenerationResult.model_validate(json.loads(result_json))

    assert result.quest.quest_id == "quest-mine-iron-ore-50"
    assert result.quest.title == "Mine 50 iron ore"
    assert result.quest.objective.target_object_id == "iron_ore_node_01"
    assert result.quest.objective.required_event == "resource_collected"
    assert result.quest.objective.target_item_id == "iron_ore"
    assert result.quest.objective.required_count == 50
    assert result.metadata["reason"] == "iron_ore_node_nearby"


def test_generates_iron_ingot_crafting_quest_json() -> None:
    service = QuestAgentService()

    result_json = service.generate_quest_json(
        {
            "event": "player_entered_area",
            "game_state": {
                "player_location": "smelting_area",
                "nearby_objects": [
                    {
                        "id": "smelter_01",
                        "type": "smelter",
                        "status": "available",
                    }
                ],
            },
        }
    )

    result = QuestGenerationResult.model_validate(json.loads(result_json))

    assert result.quest.quest_id == "quest-craft-iron-ingot-5"
    assert result.quest.title == "Craft 5 iron ingots"
    assert result.quest.objective.target_object_id == "smelter_01"
    assert result.quest.objective.required_event == "item_crafted"
    assert result.quest.objective.target_item_id == "iron_ingot"
    assert result.quest.objective.required_count == 5
    assert result.metadata["reason"] == "smelter_nearby"


def test_generates_bottleneck_machine_quest_json() -> None:
    service = QuestAgentService()

    result_json = service.generate_quest_json(
        {
            "event": "factory_state_changed",
            "game_state": {
                "player_location": "production_line",
                "machines": [
                    {
                        "id": "packaging_01",
                        "input_rate": 100,
                        "output_rate": 62,
                        "status": "running",
                    }
                ],
            },
        }
    )

    result = QuestGenerationResult.model_validate(json.loads(result_json))

    assert result.quest.quest_id == "quest-optimize-packaging_01"
    assert result.quest.title == "Inspect the bottleneck machine"
    assert result.quest.objective.target_object_id == "packaging_01"
    assert result.quest.objective.required_event == "machine_inspected"
    assert result.quest.priority == "high"
    assert result.metadata["source"] == "mock_game_state"
