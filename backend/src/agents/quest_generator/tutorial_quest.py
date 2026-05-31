"""Tutorial quest sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class TutorialQuestAgent:
    """Generate onboarding quests."""

    agent_id = "quest_generator.tutorial_quest"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"Create a tutorial quest: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(
            agent="quest_generator",
            payload={
                "quest": {
                    "type": "tutorial",
                    "title": "첫 생산 라인 점검",
                    "objective": "기본 설비의 입력과 출력을 확인하세요.",
                }
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
