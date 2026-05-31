"""Recipe explanation sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class RecipeExplainerAgent:
    """Explain recipes and production chains."""

    agent_id = "manual_qa.recipe_explainer"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 레시피 질문에 답변하세요: {payload}"

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        question = str(payload.get("question") or payload.get("message") or "")
        return AgentRunResult(
            agent="manual_qa",
            payload={
                "answer": "레시피 입력 재료, 생산 결과, 선행 조건을 순서대로 확인하세요.",
                "question": question,
                "topic": "recipe",
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
