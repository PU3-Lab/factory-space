"""생산 퀘스트 leaf agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.quest_generator.service import QuestAgentService
from agents.quest_generator.tools import (
    PRODUCTION_QUEST_SELECTION_TOOL_NAME,
    ProductionQuestSelectionTool,
)


class ProductionQuestAgent:
    """자원 채집 또는 아이템 제작 생산 퀘스트를 선택합니다."""

    agent_id = "quest_generator.production_quest"
    tools = (ProductionQuestSelectionTool(),)

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        available_quests = json.dumps(
            QuestAgentService().available_quest_json(),
            ensure_ascii=False,
        )
        game_state = json.dumps(payload.get("game_state", {}), ensure_ascii=False)
        return (
            "[ROLE]\n"
            "팩토리 스페이스 생산 퀘스트 선택 에이전트입니다.\n\n"
            "[TASK]\n"
            "퀘스트 요청이 들어오면 게임 상태에 맞는 생산 퀘스트를 자동으로 생성합니다.\n"
            "GAME_STATE가 비어 있지 않으면 그 상태(진행도, 보유 자원 등)에 가장 적합한 "
            "기존 퀘스트 id 5개를 고르세요.\n"
            "GAME_STATE가 비어 있으면 초반 진행에 적합한 대표 퀘스트 id 5개를 고르세요.\n"
            "퀘스트 제목, 설명, 목표는 새로 만들거나 고치거나 번역하지 마세요.\n"
            "고른 id는 반드시 tool_call로만 전달하세요.\n\n"
            "[TOOLS]\n"
            f"- {PRODUCTION_QUEST_SELECTION_TOOL_NAME}\n"
            '  args: {"selected_quest_ids":[정수 id 5개]}\n\n'
            "[GAME_STATE]\n"
            f"{game_state}\n\n"
            "[AVAILABLE_QUESTS]\n"
            f"{available_quests}\n\n"
            "[OUTPUT_CONTRACT]\n"
            "도구 실행 전 첫 응답은 다음 형태의 tool_call JSON 객체만 반환하세요:\n"
            '{"tool_call":{"name":"quest_generator.select_production_quests",'
            '"args":{"selected_quest_ids":[2,4,6,8,10]}}}\n'
            "위 id 값은 형식 예시일 뿐이며 그대로 따라 쓰지 마세요.\n"
            "AVAILABLE_QUESTS 안의 id만 정확히 5개, 중복 없이 사용하세요.\n"
            "GAME_STATE와 AVAILABLE_QUESTS를 보고 가장 적절한 5개 id를 고르세요.\n"
            "quests, selected_quest_ids 직접 응답, markdown, 주석, 이유, 추가 key는 포함하지 마세요."
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """LLM 응답을 사용할 수 없을 때 예시 생산 퀘스트를 반환합니다."""

        return AgentRunResult(
            agent="quest_generator",
            payload=QuestAgentService().generate_quest_json(),
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
