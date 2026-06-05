"""Economy quest leaf agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class EconomyQuestAgent:
    """Generate economy quests that improve storage or resource flow."""

    agent_id = "quest_generator.economy_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """Build the prompt used to generate economy quests with an LLM."""

        return (
            "[ROLE]\n"
            "Factory Space economy quest generator.\n\n"
            "[TASK]\n"
            "Create exactly 5 economy quests that improve storage or resource flow.\n\n"
            "[REQUEST_PAYLOAD]\n"
            f"{payload}\n\n"
            "[OUTPUT_CONTRACT]\n"
            "Return only a JSON object with this shape:\n"
            '{"quests":[{"id":1,"type":"economy","title":"",'
            '"description":"","objectives":[{"target_item_id":"",'
            '"quantity":1}]}]}\n'
            "Every quest type must be economy. quantity must be greater than 0.\n"
            "Do not include markdown, comments, or extra top-level keys."
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """Return a basic economy quest when LLM output is unavailable."""

        return AgentRunResult(
            agent="quest_generator",
            payload={
                "quest": {
                    "type": "economy",
                    "title": "Improve storage flow",
                    "objective": "Reduce one storage overload or shortage.",
                }
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
