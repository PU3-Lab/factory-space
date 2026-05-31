"""Quest generator orchestrating agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext

QUEST_SUB_AGENT_IDS = {
    "quest_generator.tutorial_quest",
    "quest_generator.production_quest",
    "quest_generator.exploration_quest",
    "quest_generator.economy_quest",
}


class QuestGeneratorAgent:
    """Select the quest generation sub-agent."""

    agent_id = "quest_generator"

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """Build the prompt used to select a quest sub-agent."""

        return (
            "You are the quest generation domain sub-orchestrator.\n"
            "Select exactly one sub-agent for the quest request.\n"
            "Allowed sub-agents: quest_generator.tutorial_quest, "
            "quest_generator.production_quest, quest_generator.exploration_quest, "
            "quest_generator.economy_quest.\n"
            "Return compact JSON only: "
            '{"sub_agent":"<allowed_sub_agent>","reason":"<short reason>"}\n'
            f"Request context: {context.metadata}\n"
            f"Request payload: {payload}"
        )

    def parse_sub_agent_selection(self, raw: str | None) -> str | None:
        """Parse a model routing decision."""

        if not raw:
            return None

        try:
            value: Any = json.loads(raw)
        except json.JSONDecodeError:
            candidate = raw.strip()
        else:
            candidate = (
                str(value.get("sub_agent", "")).strip() if isinstance(value, dict) else ""
            )

        if candidate in QUEST_SUB_AGENT_IDS:
            return candidate
        return None
