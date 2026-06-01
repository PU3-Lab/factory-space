"""Exploration quest sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class ExplorationQuestAgent:
    """Generate exploration quests."""

    agent_id = "quest_generator.exploration_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 요청을 바탕으로 탐험 퀘스트를 생성하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(
            agent="quest_generator",
            payload={
                "quest": {
                    "type": "exploration",
                    "title": "신규 자원 조사",
                    "objective": "아직 확인하지 않은 자원 후보를 조사하세요.",
                }
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
