"""LLM이 제안한 재료 스펙을 검증, 새니타이징 및 밸런싱합니다."""

from __future__ import annotations

from agents.material_generation.schemas import MaterialProposal

FORBIDDEN_KEYWORDS = {"trash", "error", "test", "dummy", "fuck", "shit", "invalid"}
VALID_RARITIES = {"common", "uncommon", "rare", "epic"}


class MaterialResultValidator:
    """재료 밸런스 제약 조건을 검증하고 안전 새니타이징을 수행합니다."""

    @classmethod
    def validate_and_correct(cls, proposal: MaterialProposal) -> MaterialProposal:
        """범위를 벗어난 속성을 수정하고, 금지된 이름을 새니타이징하며, 후보 출력 개수를 제한합니다."""
        if not proposal.result:
            return proposal

        result = proposal.result

        # 1. 속성값 클램핑 (범위 강제: 0.0 <= 속성값 <= 10.0)
        p = result.properties
        p.strength = max(0.0, min(10.0, float(p.strength)))
        p.conductivity = max(0.0, min(10.0, float(p.conductivity)))
        p.stability = max(0.0, min(10.0, float(p.stability)))
        p.reactivity = max(0.0, min(10.0, float(p.reactivity)))

        # 2. 희귀도(Rarity) 새니타이징
        rarity_clean = result.rarity.strip().lower()
        if rarity_clean not in VALID_RARITIES:
            result.rarity = "common"
        else:
            result.rarity = rarity_clean

        # 3. 금지된 단어가 포함된 이름 및 ID 힌트 새니타이징
        name_lower = result.name.lower()
        id_lower = result.id_hint.lower()

        for term in FORBIDDEN_KEYWORDS:
            if term in name_lower:
                result.name = result.name.replace(term, "alloy").replace(
                    term.capitalize(), "Alloy"
                )
            if term in id_lower:
                result.id_hint = result.id_hint.replace(term, "alloy")

        # 4. 복잡성을 피하기 위해 권장 다음 레시피 후보군을 최대 5개로 제한
        if len(result.next_recipe_candidates) > 5:
            result.next_recipe_candidates = result.next_recipe_candidates[:5]

        return proposal
