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
