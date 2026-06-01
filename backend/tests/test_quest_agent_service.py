from __future__ import annotations

from pydantic import ValidationError

from agents.quest_generator.schemas import QuestResponse
from agents.quest_generator.service import QuestAgentService


def test_service_generates_iron_ore_mining_quest_from_mock_game_state() -> None:
    service = QuestAgentService()

    result = service.generate_quest_json(
        {
            "quest_case": "mine_iron_ore_10",
            "inventory": {"iron_ore": 0},
        }
    )

    QuestResponse.model_validate(result)
    quest = result["quest"]
    assert quest["id"] == "quest_mine_iron_ore_10"
    assert quest["type"] == "production"
    assert quest["title"] == "철광석 10개 채굴"
    assert quest["objectives"] == [
        {
            "action": "mine",
            "target_item_id": "iron_ore",
            "target_item_name": "철광석",
            "quantity": 10,
        }
    ]


def test_service_generates_default_production_quest_from_empty_game_state() -> None:
    service = QuestAgentService()

    result = service.generate_quest_json({})

    QuestResponse.model_validate(result)
    quest = result["quest"]
    assert quest["id"] == "quest_factory_checkup"
    assert quest["type"] == "production"
    assert quest["objectives"][0]["action"] == "inspect"
    assert quest["objectives"][0]["quantity"] == 1


def test_quest_response_rejects_invalid_quantity() -> None:
    invalid_response = {
        "quest": {
            "id": "quest_invalid",
            "type": "production",
            "title": "invalid",
            "description": "invalid",
            "objectives": [
                {
                    "action": "mine",
                    "target_item_id": "iron_ore",
                    "target_item_name": "철광석",
                    "quantity": 0,
                }
            ],
        }
    }

    try:
        QuestResponse.model_validate(invalid_response)
    except ValidationError as exc:
        assert "greater than 0" in str(exc)
    else:
        raise AssertionError("Expected ValidationError")
