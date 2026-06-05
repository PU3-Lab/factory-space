"""New material generator agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class NewMaterialGeneratorAgent:
    """Generate a candidate material from design constraints."""

    agent_id = "new_material_generator"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return (
            f"다음 제약 조건을 바탕으로 공장 신소재 후보를 생성하세요: {payload}\n"
            "반드시 다음 JSON 스키마 형식의 JSON 객체 하나만 출력하세요. 마크다운 펜스(```json)나 부가 설명은 절대 쓰지 마세요.\n"
            "{\n"
            '  "materials": [\n'
            "    {\n"
            '      "name": "신소재 이름",\n'
            '      "role": "역할",\n'
            '      "rarity": "희귀도",\n'
            '      "production_notes": "생산 메모"\n'
            "    }\n"
            "  ]\n"
            "}"
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        goal = str(payload.get("goal") or "balanced factory material")
        return AgentRunResult(
            agent=self.agent_id,
            payload={
                "materials": [
                    {
                        "name": "Composite Catalyst",
                        "role": goal,
                        "rarity": "standard",
                        "production_notes": "초기 fallback 후보입니다.",
                    }
                ]
            },
            metadata={"fallback": True},
        )
