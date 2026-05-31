"""Orchestrator agent for selecting specialist agents."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext

TOP_LEVEL_AGENT_IDS = (
    "process_optimizer",
    "operator_guide",
    "quest_generator",
    "new_material_generator",
)


class OrchestratorAgent:
    """Select the top-level specialist agent."""

    agent_id = "orchestrator"

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
        requested_agent: str | None = None,
    ) -> str:
        """Build the prompt used to select a top-level agent."""

        agent_hint = requested_agent or "none"
        allowed_agent_ids = "\n".join(
            f"- {agent_id}" for agent_id in TOP_LEVEL_AGENT_IDS
        )
        return (
            "[ROLE]\n"
            "공장 시뮬레이션 AI 서버 전체 오케스트레이터\n\n"
            "[TASK]\n"
            "요청을 처리할 최상위 Agent id를 하나만 결정한다.\n\n"
            "[ALLOWED_AGENT_IDS]\n"
            f"{allowed_agent_ids}\n\n"
            "[REQUEST_HINT]\n"
            f"agent: {agent_hint}\n\n"
            "[REQUEST_CONTEXT]\n"
            f"{context.metadata}\n\n"
            "[REQUEST_PAYLOAD]\n"
            f"{payload}\n\n"
            "[OUTPUT_CONTRACT]\n"
            "ALLOWED_AGENT_IDS 중 하나의 id만 그대로 출력한다.\n"
            "JSON, markdown, 설명, reason, 따옴표, 코드블록은 출력하지 않는다."
        )
