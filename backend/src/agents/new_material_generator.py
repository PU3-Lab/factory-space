"""New material generator agent."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult


class NewMaterialGeneratorAgent:
    """Generate a candidate material from design constraints."""

    agent_id = "new_material_generator"

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return f"다음 제약 조건을 바탕으로 공장 신소재 후보를 생성하세요: {payload}"

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
