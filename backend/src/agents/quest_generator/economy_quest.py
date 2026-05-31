"""Economy quest sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class EconomyQuestAgent:
    """Generate economy and inventory quests."""

    agent_id = "quest_generator.economy_quest"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"Create an economy quest: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(
            agent="quest_generator",
            payload={
                "quest": {
                    "type": "economy",
                    "title": "재고 흐름 개선",
                    "objective": "재고 과잉 또는 부족 항목을 하나 줄이세요.",
                }
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
