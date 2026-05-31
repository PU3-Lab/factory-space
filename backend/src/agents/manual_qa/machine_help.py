"""Machine help sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class MachineHelpAgent:
    """Explain machine state, usage, and UI context."""

    agent_id = "manual_qa.machine_help"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"Answer this machine help question: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        question = str(payload.get("question") or payload.get("message") or "")
        return AgentRunResult(
            agent="manual_qa",
            payload={
                "answer": "선택한 설비의 상태값, 입력/출력 연결, 사용 가능한 레시피를 확인하세요.",
                "question": question,
                "topic": "machine",
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
