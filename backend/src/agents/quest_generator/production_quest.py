"""Production quest sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class ProductionQuestAgent:
    """Generate production improvement quests."""

    agent_id = "quest_generator.production_quest"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"Create a production quest: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(
            agent="quest_generator",
            payload={
                "quest": {
                    "type": "production",
                    "title": "생산량 안정화",
                    "objective": "핵심 생산 라인의 처리량을 안정화하세요.",
                }
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
