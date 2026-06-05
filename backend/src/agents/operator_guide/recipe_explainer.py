"""Recipe explanation sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class RecipeExplainerAgent:
    """Explain recipes and production chains."""

    agent_id = "operator_guide.recipe_explainer"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return (
            f"다음 레시피 질문에 답변하세요: {payload}\n"
            "반드시 다음 JSON 스키마 형식의 JSON 객체 하나만 출력하세요. 마크다운 펜스(```json)나 부가 설명은 절대 쓰지 마세요.\n"
            "{\n"
            '  "answer": "답변 내용",\n'
            '  "question": "질문 내용",\n'
            '  "topic": "recipe"\n'
            "}"
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        question = str(payload.get("question") or payload.get("message") or "")
        return AgentRunResult(
            agent="operator_guide",
            payload={
                "answer": "레시피 입력 재료, 생산 결과, 선행 조건을 순서대로 확인하세요.",
                "question": question,
                "topic": "recipe",
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
