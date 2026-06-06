"""퀘스트 에이전트가 LLM tool_call로 사용할 도구입니다."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext
from agents.quest_generator.service import QuestAgentService

PRODUCTION_QUEST_SELECTION_TOOL_NAME = "quest_generator.select_production_quests"


class ProductionQuestSelectionTool:
    """LLM이 고른 퀘스트 id를 기존 production quest payload로 변환합니다."""

    name = PRODUCTION_QUEST_SELECTION_TOOL_NAME

    def invoke(
        self,
        payload: dict[str, Any],
        context: AgentContext,
        args: dict[str, Any] | None = None,
    ) -> object:
        selected_ids = (args or {}).get("selected_quest_ids")
        if not isinstance(selected_ids, list) or not all(
            type(quest_id) is int for quest_id in selected_ids
        ):
            return {
                "status": "error",
                "code": "INVALID_QUEST_SELECTION",
                "message": "selected_quest_ids는 정수 id 목록이어야 합니다.",
            }

        try:
            return QuestAgentService().generate_quest_json_from_ids(selected_ids)
        except ValueError as exc:
            return {
                "status": "error",
                "code": "INVALID_QUEST_SELECTION",
                "message": str(exc),
            }
