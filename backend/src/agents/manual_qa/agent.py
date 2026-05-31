"""Manual Q&A orchestrating agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext

MANUAL_QA_SUB_AGENT_IDS = {
    "manual_qa.recipe_explainer",
    "manual_qa.machine_help",
    "manual_qa.troubleshooter",
}


class ManualQaAgent:
    """Select the manual Q&A sub-agent for a user question."""

    agent_id = "manual_qa"

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """Build the prompt used to select a manual Q&A sub-agent."""

        return (
            "당신은 매뉴얼 Q&A 도메인 서브 오케스트레이터입니다.\n"
            "사용자 요청을 처리할 서브 에이전트를 정확히 하나 선택하세요.\n"
            "허용 서브 에이전트: manual_qa.recipe_explainer, "
            "manual_qa.machine_help, manual_qa.troubleshooter.\n"
            "반드시 다음 compact JSON만 반환하세요: "
            '{"sub_agent":"<허용된_서브_에이전트>","reason":"<짧은 이유>"}\n'
            f"요청 context: {context.metadata}\n"
            f"요청 payload: {payload}"
        )

    def parse_sub_agent_selection(self, raw: str | None) -> str | None:
        """Parse a model routing decision."""

        if not raw:
            return None

        try:
            value: Any = json.loads(raw)
        except json.JSONDecodeError:
            candidate = raw.strip()
        else:
            candidate = (
                str(value.get("sub_agent", "")).strip() if isinstance(value, dict) else ""
            )

        if candidate in MANUAL_QA_SUB_AGENT_IDS:
            return candidate
        return None
