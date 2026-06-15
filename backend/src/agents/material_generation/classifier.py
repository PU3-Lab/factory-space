"""LLM 개입 전에 알 수 없는 실험을 분류하기 위한 규칙 엔진입니다."""

from __future__ import annotations

from typing import Any

from sqlalchemy.orm import Session

from agents.material_generation.recipe_repository import RecipeRepository


class ExperimentClassifier:
    """유효하지 않거나 레시피에 없는 조합을 결정론적 결과 또는 LLM 후보군으로 분류합니다."""

    @classmethod
    def classify(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
    ) -> str:
        """실험 설정을 분류합니다.

        'simple_variation', 'intermediate_material', 'failed_result',
        'candidate', 'ambiguous' 중 하나를 반환합니다.
        """
        # 레시피 캐시가 로드되었는지 확인
        recipes = RecipeRepository.get_recipes(session)

        input_items = {item["item_id"] for item in normalized_inputs}

        # 1. simple_variation 확인:
        # 동일한 머신 유형 및 동일한 재료 세트이지만 수량이 다른 경우
        for r in recipes:
            if r.machine_type != machine_type:
                continue

            r_items = set()
            if r.input_item_1:
                r_items.add(r.input_item_1)
            if r.input_item_2:
                r_items.add(r.input_item_2)
            if r.input_item_3:
                r_items.add(r.input_item_3)

            if r_items == input_items:
                return "simple_variation"

        # 2. failed_result 확인:
        # 용광로(Smelter)에서 2개 이상의 아이템을 섞는 것은 유효하지 않음 (용광로는 기본 열처리 장치이며 합성을 하지 않음)
        if len(input_items) >= 2 and machine_type == "Smelter":
            return "failed_result"

        # 3. intermediate_material 확인:
        # 2개 이상의 재료를 함께 분쇄(Grinding)하면 혼합 분말(중간 재료)이 됨
        if len(input_items) >= 2 and machine_type == "Grinder":
            return "intermediate_material"

        # 4. candidate 확인:
        # 2개 이상의 아이템을 사용하는 합성기(Synthesizer)는 새로운 합금이나 화합물의 유력한 후보임
        if len(input_items) >= 2 and machine_type == "Synthesizer":
            return "candidate"

        # 단일 재료 또는 기타 모호한 예외 케이스에 대한 기본 폴백
        return "ambiguous"
