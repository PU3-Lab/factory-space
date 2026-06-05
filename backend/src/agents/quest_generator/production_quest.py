"""Production quest leaf agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.quest_generator.service import QuestAgentService


class ProductionQuestAgent:
    """Generate production quests for gathering resources or crafting items."""

    agent_id = "quest_generator.production_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """Build the prompt used to generate production quests with an LLM."""

        available_quests = json.dumps(
            QuestAgentService().available_quest_json(),
            ensure_ascii=False,
        )
        return (
            "[ROLE]\n"
            "Factory Space production quest selector.\n\n"
            "[TASK]\n"
            "Choose exactly 5 quest ids from AVAILABLE_QUESTS.\n"
            "Do not create, rewrite, translate, or modify any quest content.\n\n"
            "[AVAILABLE_QUESTS]\n"
            f"{available_quests}\n\n"
            "[REQUEST_PAYLOAD]\n"
            f"{payload}\n\n"
            "[OUTPUT_CONTRACT]\n"
            "Return only a JSON object with this shape:\n"
            '{"selected_quest_ids":[1,2,3,4,5]}\n'
            "Use exactly 5 unique ids from AVAILABLE_QUESTS.\n"
            "Do not include quests, markdown, comments, reasons, or extra keys."
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """Return example production quests when LLM output is unavailable."""

        return AgentRunResult(
            agent="quest_generator",
            payload=QuestAgentService().generate_quest_json(),
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
