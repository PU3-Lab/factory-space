"""생산과 제작 목표를 퀘스트로 만들어 주는 하위 에이전트 파일입니다."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.quest_generator.service import QuestAgentService


class ProductionQuestAgent:
    """플레이어가 자원을 모으거나 아이템을 만들도록 생산 퀘스트를 생성합니다."""

    agent_id = "quest_generator.production_quest"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """LLM에게 생산 퀘스트를 만들어 달라고 요청하는 문장을 만듭니다."""

        return f"다음 요청을 바탕으로 생산 퀘스트를 생성하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """LLM을 쓰지 못할 때 서버에 준비된 예제 생산 퀘스트를 반환합니다."""

        return AgentRunResult(
            agent="quest_generator",
            payload=QuestAgentService().generate_quest_json(),
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
