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
            "당신은 공장 시뮬레이션 AI의 서버 전체 오케스트레이터입니다.\n"
            "요청을 처리할 최상위 에이전트를 정확히 하나 선택하세요.\n"
            "허용 에이전트: process_optimizer, manual_qa, quest_generator, "
            "new_material_generator.\n"
            "반드시 다음 compact JSON만 반환하세요: "
            '{"agent":"<허용된_에이전트>","reason":"<짧은 이유>"}\n'
            f"요청 context: {context.metadata}\n"
            f"요청 payload: {payload}"
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
