"""Orchestrator agent for selecting specialist agents."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext

TOP_LEVEL_AGENT_IDS = {
    "process_optimizer",
    "manual_qa",
    "quest_generator",
    "new_material_generator",
}


class OrchestratorAgent:
    """Select the top-level specialist agent."""

    agent_id = "orchestrator"

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """Build the prompt used to select a top-level agent."""

        return (
            "You are the server-level orchestrator for a factory simulation AI.\n"
            "Select exactly one top-level agent for the request.\n"
            "Allowed agents: process_optimizer, manual_qa, quest_generator, "
            "new_material_generator.\n"
            "Return compact JSON only: "
            '{"agent":"<allowed_agent>","reason":"<short reason>"}\n'
            f"Request context: {context.metadata}\n"
            f"Request payload: {payload}"
        )

    def parse_agent_selection(self, raw: str | None) -> str | None:
        """Parse a model routing decision."""

        if not raw:
            return None

        try:
            value: Any = json.loads(raw)
        except json.JSONDecodeError:
            candidate = raw.strip()
        else:
            candidate = str(value.get("agent", "")).strip() if isinstance(value, dict) else ""

        if candidate in TOP_LEVEL_AGENT_IDS:
            return candidate
        return None
