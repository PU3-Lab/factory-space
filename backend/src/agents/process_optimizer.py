"""Process optimizer agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class ProcessOptimizerAgent:
    """Suggest process improvements from a factory snapshot."""

    agent_id = "process_optimizer"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"공장 snapshot에서 공정 병목과 개선 우선순위를 찾아주세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        machines = payload.get("machines") or payload.get("factory_state", {}).get("machines", [])
        bottleneck_count = len(machines) if isinstance(machines, list) else 0
        return AgentRunResult(
            agent=self.agent_id,
            payload={
                "summary": "공정 상태를 기준으로 우선 점검 항목을 생성했습니다.",
                "recommendations": [
                    {
                        "title": "병목 설비 점검",
                        "priority": "high",
                        "reason": f"{bottleneck_count}개 설비 snapshot을 기준으로 점검합니다.",
                    }
                ],
            },
            metadata={"fallback": True},
        )
