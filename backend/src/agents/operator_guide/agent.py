"""Operator guide orchestrating agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext

OPERATOR_GUIDE_LEAF_AGENT_IDS = (
    "operator_guide.recipe_explainer",
    "operator_guide.machine_help",
    "operator_guide.troubleshooter",
)


class OperatorGuideAgent:
    """Select the operator guide leaf agent for a user question."""

    agent_id = "operator_guide"
    tools = ()

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """Build the prompt used to select an operator guide leaf agent."""

        allowed_leaf_agent_ids = "\n".join(
            f"- {sub_agent_id}" for sub_agent_id in OPERATOR_GUIDE_LEAF_AGENT_IDS
        )
        return (
            "[ROLE]\n"
            "운영자 가이드 도메인 오케스트레이터\n\n"
            "[TASK]\n"
            "사용자 요청을 처리할 leaf Agent id를 하나만 결정한다.\n\n"
            "[ALLOWED_LEAF_AGENT_IDS]\n"
            f"{allowed_leaf_agent_ids}\n\n"
            "[REQUEST_CONTEXT]\n"
            f"{context.metadata}\n\n"
            "[REQUEST_PAYLOAD]\n"
            f"{payload}\n\n"
            "[OUTPUT_CONTRACT]\n"
            "ALLOWED_LEAF_AGENT_IDS 중 하나의 id만 그대로 출력한다.\n"
            "JSON, markdown, 설명, reason, 따옴표, 코드블록은 출력하지 않는다."
        )
