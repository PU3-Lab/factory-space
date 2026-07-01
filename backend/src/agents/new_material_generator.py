"""New material generator agent.

Goal-based material generator: given free-form design constraints (a goal,
target properties, etc.), it asks the LLM to invent candidate new materials and
returns them as a ``materials`` array. This is distinct from the recipe-driven
``material_generation`` agent, which synthesizes a single material from a
machine type and concrete input items.
"""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.material_columns import get_unreal_material_column_values


class NewMaterialGeneratorAgent:
    """Generate candidate materials from design constraints."""

    agent_id = "new_material_generator"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        return (
            f"다음 제약 조건을 바탕으로 공장 신소재 후보를 생성하세요: {payload}\n"
            "반드시 다음 JSON 스키마 형식의 JSON 객체 하나만 출력하세요. "
            "마크다운 펜스(```json)나 부가 설명은 절대 쓰지 마세요.\n"
            "{\n"
            '  "materials": [\n'
            "    {\n"
            '      "name": "신소재 이름",\n'
            '      "role": "역할",\n'
            '      "rarity": "희귀도",\n'
            '      "production_notes": "생산 메모",\n'
            '      "rowname": "CSV/DataTable 행 식별자",\n'
            '      "form": "물질의 물리 상태",\n'
            '      "substance": "기본 성분 또는 재료 계열",\n'
            '      "type": "물질 분류",\n'
            '      "shape": "물질 형태",\n'
            '      "DIsplayName": "UI에 표시할 한국어 이름",\n'
            '      "VisualColor": "언리얼 RGBA 색상 문자열"\n'
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
                        **get_unreal_material_column_values(),
                    }
                ]
            },
            metadata={"fallback": True},
        )
