"""Production quest sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.quest_generator.service import QuestAgentService


class ProductionQuestAgent:
    """Generate production improvement quests."""

    agent_id = "quest_generator.production_quest"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 요청을 바탕으로 생산 퀘스트를 생성하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(
            agent="quest_generator",
            payload=QuestAgentService().generate_quest_json(),
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
