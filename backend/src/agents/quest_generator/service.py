"""Quest generation service."""

from __future__ import annotations

import random
from typing import Any

from agents.quest_generator.schemas import Quest, QuestResponse

_EXAMPLE_QUESTS: tuple[dict[str, Any], ...] = (
    {
        "id": 1,
        "type": "production",
        "title": "철광석 10개 채굴",
        "description": "기초 생산 라인을 준비하기 위해 철광석 10개를 채굴하세요.",
        "objectives": [{"target_item_id": "iron_ore", "quantity": 10}],
    },
    {
        "id": 2,
        "type": "production",
        "title": "구리광석 8개 채굴",
        "description": "전력 설비 확장을 위해 구리광석 8개를 확보하세요.",
        "objectives": [{"target_item_id": "copper_ore", "quantity": 8}],
    },
    {
        "id": 3,
        "type": "production",
        "title": "철판 5개 생산",
        "description": "초기 제작 재료로 사용할 철판 5개를 생산하세요.",
        "objectives": [{"target_item_id": "iron_plate", "quantity": 5}],
    },
    {
        "id": 4,
        "type": "production",
        "title": "구리선 12개 생산",
        "description": "배선 작업에 필요한 구리선 12개를 생산하세요.",
        "objectives": [{"target_item_id": "copper_wire", "quantity": 12}],
    },
    {
        "id": 5,
        "type": "production",
        "title": "석탄 6개 확보",
        "description": "발전 설비 가동을 위해 석탄 6개를 확보하세요.",
        "objectives": [{"target_item_id": "coal", "quantity": 6}],
    },
    {
        "id": 6,
        "type": "production",
        "title": "기어 4개 제작",
        "description": "기계 조립에 필요한 기어 4개를 제작하세요.",
        "objectives": [{"target_item_id": "gear", "quantity": 4}],
    },
)


class QuestAgentService:
    """Generate validated quests from the server-side example quest pool."""

    def __init__(self, rng: random.Random | None = None) -> None:
        self._rng = rng or random.SystemRandom()

    def generate_quest_json(self, count: int = 5) -> dict[str, Any]:
        """Return JSON-serializable quests after Pydantic validation."""

        selected_quests = self._rng.sample(_EXAMPLE_QUESTS, k=count)
        quests = [Quest.model_validate(quest) for quest in selected_quests]
        return QuestResponse(quests=quests).model_dump(mode="json")
