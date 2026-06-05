"""Troubleshooting sub-agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class TroubleshooterAgent:
    """Diagnose blocked production and error states."""

    agent_id = "operator_guide.troubleshooter"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return (
            f"다음 공장 문제를 진단하고 해결 순서를 제안하세요: {payload}\n"
            "반드시 다음 JSON 스키마 형식의 JSON 객체 하나만 출력하세요. 마크다운 펜스(```json)나 부가 설명은 절대 쓰지 마세요.\n"
            "{\n"
            '  "answer": "답변 내용",\n'
            '  "question": "질문 내용",\n'
            '  "topic": "troubleshooting"\n'
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
                "answer": "입력 재료 부족, 출력 저장소 포화, 전력 부족, 설비 정지 상태를 순서대로 확인하세요.",
                "question": question,
                "topic": "troubleshooting",
            },
            metadata={"fallback": True, "sub_agent": self.agent_id},
        )
