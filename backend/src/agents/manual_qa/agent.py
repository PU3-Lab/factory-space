"""Manual Q&A orchestrating agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext

MANUAL_QA_SUB_AGENT_IDS = {
    "manual_qa.recipe_explainer",
    "manual_qa.machine_help",
    "manual_qa.troubleshooter",
}


class ManualQaAgent:
    """Select the manual Q&A sub-agent for a user question."""

    agent_id = "manual_qa"

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """Build the prompt used to select a manual Q&A sub-agent."""

        return (
            "You are the manual Q&A domain sub-orchestrator.\n"
            "Select exactly one sub-agent for the user request.\n"
            "Allowed sub-agents: manual_qa.recipe_explainer, "
            "manual_qa.machine_help, manual_qa.troubleshooter.\n"
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

        if candidate in MANUAL_QA_SUB_AGENT_IDS:
            return candidate
        return None
