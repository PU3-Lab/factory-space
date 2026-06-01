"""Quest generation service."""

from __future__ import annotations

from typing import Any

from agents.quest_generator.schemas import Quest, QuestObjective, QuestResponse


class QuestAgentService:
    """Generate validated quests from a mock game state."""

    def generate_quest_json(self, game_state: dict[str, Any]) -> dict[str, Any]:
        """Return a JSON-serializable quest payload after Pydantic validation."""

        quest = self._build_quest(game_state)
        return QuestResponse(quest=quest).model_dump(mode="json")

    def _build_quest(self, game_state: dict[str, Any]) -> Quest:
        quest_case = str(game_state.get("quest_case") or "")
        if quest_case == "mine_iron_ore_10":
            return Quest(
                id="quest_mine_iron_ore_10",
                type="production",
                title="철광석 10개 채굴",
                description="기초 생산 라인을 준비하기 위해 철광석 10개를 채굴하세요.",
                objectives=[
                    QuestObjective(
                        action="mine",
                        target_item_id="iron_ore",
                        target_item_name="철광석",
                        quantity=10,
                    )
                ],
            )

        return Quest(
            id="quest_factory_checkup",
            type="production",
            title="생산 라인 점검",
            description="현재 공장 상태를 확인하고 다음 생산 목표를 준비하세요.",
            objectives=[
                QuestObjective(
                    action="inspect",
                    target_item_id="factory_line",
                    target_item_name="생산 라인",
                    quantity=1,
                )
            ],
        )
