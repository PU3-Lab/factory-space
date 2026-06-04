"""Troubleshooting sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.service import build_manual_qa_agent_result


class TroubleshooterAgent:
    """Diagnose blocked production and error states."""

    agent_id = "operator_guide.troubleshooter"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 공장 문제를 진단하고 해결 순서를 제안하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return build_manual_qa_agent_result(
            payload,
            context,
            topic="troubleshooting",
            sub_agent=self.agent_id,
        )
