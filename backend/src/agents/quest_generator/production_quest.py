"""Production quest sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.quest_generator.service import QuestAgentService


class ProductionQuestAgent:
    """Generate production improvement quests."""

    agent_id = "quest_generator.production_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return (
            f"다음 요청을 바탕으로 생산 퀘스트를 생성하세요: {payload}\n"
            "반드시 다음 JSON 스키마 형식의 JSON 객체 하나만 출력하세요. 마크다운 펜스(```json)나 부가 설명은 절대 쓰지 마세요.\n"
            "{\n"
            '  "quests": [\n'
            "    {\n"
            '      "id": 1,\n'
            '      "type": "production",\n'
            '      "title": "퀘스트 제목",\n'
            '      "description": "퀘스트 설명",\n'
            '      "objectives": [\n'
            "        {\n"
            '          "target_item_id": "대상 아이템 ID",\n'
            '          "quantity": 10\n'
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}"
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(
            agent="quest_generator",
            payload=QuestAgentService().generate_quest_json(),
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
