"""Economy quest sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class EconomyQuestAgent:
    """Generate economy and inventory quests."""

    agent_id = "quest_generator.economy_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return (
            f"다음 요청을 바탕으로 경제 퀘스트를 생성하세요: {payload}\n"
            "반드시 다음 JSON 스키마 형식의 JSON 객체 하나만 출력하세요. 마크다운 펜스(```json)나 부가 설명은 절대 쓰지 마세요.\n"
            "{\n"
            '  "quest": {\n'
            '    "type": "economy",\n'
            '    "title": "퀘스트 제목",\n'
            '    "objective": "퀘스트 목표"\n'
            "  }\n"
            "}"
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(
            agent="quest_generator",
            payload={
                "quest": {
                    "type": "economy",
                    "title": "재고 흐름 개선",
                    "objective": "재고 과잉 또는 부족 항목을 하나 줄이세요.",
                }
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
