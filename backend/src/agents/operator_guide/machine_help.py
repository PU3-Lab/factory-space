"""Machine help sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.service import build_manual_qa_agent_result


class MachineHelpAgent:
    """Explain machine state, usage, and UI context."""

    agent_id = "operator_guide.machine_help"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 설비 도움말 질문에 답변하세요: {payload}"

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
