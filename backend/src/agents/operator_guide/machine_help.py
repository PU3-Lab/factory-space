"""Machine help sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.service import (
    build_manual_qa_agent_result,
    build_manual_qa_prompt,
    build_manual_qa_prompt_messages,
)


class MachineHelpAgent:
    """Explain machine state, usage, and UI context."""

    agent_id = "operator_guide.machine_help"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt(
            question,
            topic="machine",
            sub_agent=self.agent_id,
            context=context.metadata,
        )

    def build_prompt_messages(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> list[dict[str, str]]:
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt_messages(
            question,
            topic="machine",
            sub_agent=self.agent_id,
            context=context.metadata,
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return build_manual_qa_agent_result(
            payload,
            context,
            topic="machine",
            sub_agent=self.agent_id,
        )
