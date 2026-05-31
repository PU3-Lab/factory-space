"""Machine help sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class MachineHelpAgent:
    """Explain machine state, usage, and UI context."""

    agent_id = "operator_guide.machine_help"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 설비 도움말 질문에 답변하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        question = str(payload.get("question") or payload.get("message") or "")
        return AgentRunResult(
            agent="operator_guide",
            payload={
                "answer": "선택한 설비의 상태값, 입력/출력 연결, 사용 가능한 레시피를 확인하세요.",
                "question": question,
                "topic": "machine",
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
