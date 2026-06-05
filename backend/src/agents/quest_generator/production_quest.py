"""생산 퀘스트 leaf agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.quest_generator.service import QuestAgentService


class ProductionQuestAgent:
    """자원 채집 또는 아이템 제작 생산 퀘스트를 선택합니다."""

    agent_id = "quest_generator.production_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        available_quests = json.dumps(
            QuestAgentService().available_quest_json(),
            ensure_ascii=False,
        )
        return (
            "[ROLE]\n"
            "팩토리 스페이스 생산 퀘스트 선택 에이전트입니다.\n\n"
            "[TASK]\n"
            "AVAILABLE_QUESTS에 있는 기존 퀘스트 id 중 정확히 5개를 고르세요.\n"
            "퀘스트 제목, 설명, 목표는 새로 만들거나 고치거나 번역하지 마세요.\n\n"
            "[AVAILABLE_QUESTS]\n"
            f"{available_quests}\n\n"
            "[REQUEST_PAYLOAD]\n"
            f"{payload}\n\n"
            "[OUTPUT_CONTRACT]\n"
            "다음 형태의 JSON 객체만 반환하세요:\n"
            '{"selected_quest_ids":[1,2,3,4,5]}\n'
            "AVAILABLE_QUESTS 안의 id만 정확히 5개, 중복 없이 사용하세요.\n"
            "quests, markdown, 주석, 이유, 추가 key는 포함하지 마세요."
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
