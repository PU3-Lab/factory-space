"""재고와 자원 흐름을 개선하는 경제 퀘스트 하위 에이전트 파일입니다."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class EconomyQuestAgent:
    """창고 과잉이나 자원 부족처럼 경제 흐름과 관련된 퀘스트를 생성합니다."""

    agent_id = "quest_generator.economy_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """LLM에게 경제 흐름을 개선하는 퀘스트를 만들어 달라고 요청하는 문장을 만듭니다."""

        return f"다음 요청을 바탕으로 경제 퀘스트를 생성하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """LLM을 쓰지 못할 때 기본 경제 퀘스트 한 개를 반환합니다."""

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
