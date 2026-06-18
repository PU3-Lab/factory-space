"""경제 퀘스트 leaf agent."""

from __future__ import annotations

import json
from typing import Any

from agents.base import AgentContext, AgentRunResult


class EconomyQuestAgent:
    """창고 또는 자원 흐름을 개선하는 경제 퀘스트를 만듭니다."""

    agent_id = "quest_generator.economy_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        game_state = json.dumps(payload.get("game_state", {}), ensure_ascii=False)
        return (
            "팩토리 스페이스 경제 퀘스트 생성 에이전트입니다.\n"
            "퀘스트 요청이 들어오면 게임 상태에 맞는 경제 퀘스트를 자동으로 생성하세요.\n"
            "GAME_STATE가 비어 있지 않으면 그 상태(재고, 자원 흐름 등)에 맞는 퀘스트를, "
            "비어 있으면 재고/자원 흐름을 개선하는 대표 경제 퀘스트를 생성하세요.\n"
            f"[GAME_STATE]\n{game_state}\n"
            "반드시 다음 JSON 스키마 형식의 JSON 객체 하나만 출력하세요. "
            "마크다운 코드 펜스(```json)나 부가 설명은 넣지 마세요.\n"
            "퀘스트 제목과 목표 문장은 한글로 작성하세요.\n"
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
        """LLM 응답을 사용할 수 없을 때 기본 경제 퀘스트를 반환합니다."""

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
