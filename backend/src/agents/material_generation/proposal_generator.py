"""후보 재료들에 대한 LLM MaterialProposalGenerator(재료 제안 생성기)입니다."""

from __future__ import annotations

import json
import logging
from typing import Any

from agents.material_generation.schemas import (
    MaterialProperties,
    MaterialProposal,
    MaterialProposalResult,
    ProcessConditionsSchema,
)
from llm.adapter import create_llm_adapter
from llm.settings import LLMSettings

logger = logging.getLogger(__name__)


class MaterialProposalGenerator:
    """폴백(fallback) 지원을 바탕으로 LLM을 통해 재료 제안 콘셉트를 생성합니다."""

    def __init__(self) -> None:
        self.settings = LLMSettings.from_env()
        # 기본 슬롯에서 LLM 어댑터 초기화
        self.adapter = create_llm_adapter(self.settings.default)

    def _build_prompt(
        self,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
        process_conditions: ProcessConditionsSchema,
        similar_experiments: list[dict[str, Any]],
    ) -> str:
        """LLM을 위한 지시 프롬프트를 구성합니다."""
        inputs_str = ", ".join(
            f"{item['item_id']} (qty: {item['qty']})" for item in normalized_inputs
        )

        similar_context = ""
        if similar_experiments:
            similar_context = "참고 가능한 유사 실험 및 합성 성공 기록:\n"
            for i, exp in enumerate(similar_experiments, 1):
                exp_inputs = ", ".join(
                    f"{item['item_id']}:{item['qty']}" for item in exp["inputs"]
                )
                similar_context += (
                    f"  {i}. 입력: {exp_inputs} -> 생성 물질: '{exp['material_name']}' "
                    f"(카테고리: {exp['material_category']}, 속성: {exp['properties']})\n"
                )
        else:
            similar_context = "참고 가능한 이전 성공 기록이 없습니다. 새로운 속성을 창의적으로 설계하십시오.\n"

        prompt = (
            "당신은 공장의 신소재를 설계하고 분석하는 수석 재료공학 AI 에이전트입니다.\n"
            "플레이어가 다음과 같이 새로운 실험적 합성 조합을 시도했습니다.\n\n"
            f"- 장비: {machine_type}\n"
            f"- 입력 재료 조합: {inputs_str}\n"
            f"- 공정 조건: 온도={process_conditions.temperature}, 압력={process_conditions.pressure}, 촉매={process_conditions.catalyst or '없음'}\n\n"
            f"{similar_context}\n"
            "이 조합의 과학적, 공학적 의미를 분석하여 적절한 신소재 초안을 제안해 주십시오.\n"
            "결과는 반드시 아래의 JSON 포맷 형식을 엄격하게 준수하여 하나의 JSON 객체로만 반환하십시오.\n"
            "마크다운 펜스(```json)나 부가 설명은 절대 쓰지 마십시오.\n\n"
            "{\n"
            '  "proposal_type": "new_material",\n'
            '  "confidence": 0.85,\n'
            '  "reason": "합성이 타당한 공학적 사유 설명",\n'
            '  "result": {\n'
            '    "id_hint": "alloy_iron_copper",\n'
            '    "name": "제안된 신소재 한글 명칭",\n'
            '    "category": "alloy 또는 chemical 또는 organic 또는 composite 등",\n'
            '    "rarity": "common 또는 uncommon 또는 rare 또는 epic",\n'
            '    "description": "세계관에 부합하는 멋지고 상세한 설명",\n'
            '    "properties": {\n'
            '      "strength": 5.0,\n'
            '      "conductivity": 5.0,\n'
            '      "stability": 5.0,\n'
            '      "reactivity": 5.0\n'
            "    },\n"
            '    "risks": ["oxidation", "instability" 등 발생 가능한 리스크 리스트],\n'
            '    "usage": ["전선", "프레임" 등 주요 활용처 태그 리스트],\n'
            '    "next_recipe_candidates": ["다음 공정에 활용할 수 있는 가상의 아이템 ID 후보군"],\n'
            '    "visual_prompt": "아이콘 및 2D 텍스처 생성을 위한 영문 프롬프트 (예: \'reddish brown metal ingot, sci-fi style\')"\n'
            "  }\n"
            "}\n"
            "주의: properties의 4가지 값(strength, conductivity, stability, reactivity)은 반드시 0.0 ~ 10.0 사이의 실수로 정의하십시오.\n"
        )
        return prompt

    def _parse_llm_response(self, text: str) -> MaterialProposal:
        """원시 LLM 문자열을 파싱하여 검증된 MaterialProposal 스키마로 변환합니다."""
        cleaned = text.strip()
        # 마크다운 코드 펜스가 있는 경우 이를 제거합니다.
        if cleaned.startswith("```"):
            lines = cleaned.splitlines()
            if lines[0].startswith("```"):
                lines = lines[1:]
            if lines and lines[-1].startswith("```"):
                lines = lines[:-1]
            cleaned = "\n".join(lines).strip()

        data = json.loads(cleaned)
        return MaterialProposal.model_validate(data)

    def generate_proposal(
        self,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
        process_conditions: ProcessConditionsSchema,
        similar_experiments: list[dict[str, Any]],
    ) -> MaterialProposal:
        """제안을 얻기 위해 LLM을 호출하며, 실패 시 강력한 폴백을 실행합니다."""
        prompt = self._build_prompt(
            machine_type, normalized_inputs, process_conditions, similar_experiments
        )

        try:
            logger.info("Invoking LLM for material generation proposal...")
            raw_response = self.adapter.invoke(prompt)
            if raw_response:
                return self._parse_llm_response(raw_response)
        except Exception as exc:
            logger.warning("LLM proposal generation failed: %s. Using fallback.", exc)

        return self.get_fallback_proposal(normalized_inputs)

    def get_fallback_proposal(
        self, normalized_inputs: list[dict[str, Any]]
    ) -> MaterialProposal:
        """LLM이 오프라인이거나 실패할 경우 기본적이고 안전한 폴백 제안을 생성합니다."""
        names = [
            item["item_id"].replace("_ingot", "").replace("_powder", "").title()
            for item in normalized_inputs
        ]
        compound_name = " ".join(names)
        id_hint = "_".join(
            item["item_id"].replace("_ingot", "").replace("_powder", "")
            for item in normalized_inputs
        )

        return MaterialProposal(
            proposal_type="new_material",
            confidence=0.5,
            reason="LLM 생성 실패 또는 비가용 상태로 인한 기본 fallback 동작",
            result=MaterialProposalResult(
                id_hint=f"mat_{id_hint}_alloy",
                name=f"{compound_name} 합금 (Fallback)",
                category="alloy",
                rarity="common",
                description="기본 합성 공정으로 생성된 대체 합금 물질입니다.",
                properties=MaterialProperties(
                    strength=5.0,
                    conductivity=5.0,
                    stability=5.0,
                    reactivity=2.0,
                ),
                risks=["none"],
                usage=["basic_structure"],
                next_recipe_candidates=[],
                visual_prompt=f"ingot of {compound_name} alloy, metallic texture, simple icon",
            ),
        )
