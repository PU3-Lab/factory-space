"""Troubleshooting sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class TroubleshooterAgent:
    """Diagnose blocked production and error states."""

    agent_id = "operator_guide.troubleshooter"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 공장 문제를 진단하고 해결 순서를 제안하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        question = str(payload.get("question") or payload.get("message") or "")
        return AgentRunResult(
            agent="operator_guide",
            payload={
                "answer": "입력 재료 부족, 출력 저장소 포화, 전력 부족, 설비 정지 상태를 순서대로 확인하세요.",
                "question": question,
                "topic": "troubleshooting",
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
