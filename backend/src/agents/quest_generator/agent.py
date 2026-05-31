"""Quest generator orchestrating agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext

QUEST_SUB_AGENT_IDS = {
    "quest_generator.tutorial_quest",
    "quest_generator.production_quest",
    "quest_generator.exploration_quest",
    "quest_generator.economy_quest",
}


class QuestGeneratorAgent:
    """Select the quest generation sub-agent."""

    agent_id = "quest_generator"

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """Build the prompt used to select a quest sub-agent."""

        return (
            "당신은 퀘스트 생성 도메인 서브 오케스트레이터입니다.\n"
            "퀘스트 요청을 처리할 서브 에이전트를 정확히 하나 선택하세요.\n"
            "허용 서브 에이전트: quest_generator.tutorial_quest, "
            "quest_generator.production_quest, quest_generator.exploration_quest, "
            "quest_generator.economy_quest.\n"
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

        if candidate in QUEST_SUB_AGENT_IDS:
            return candidate
        return None
