"""QuestValidator for checking the validity and prerequisites of support quest drafts."""

from __future__ import annotations

from agents.quest_generator import game_data
from agents.quest_generator.models import (
    QuestContext,
    SupportQuestDraft,
    ValidationResult,
)


class QuestValidator:
    """생성된 지원 퀘스트 초안의 수량 범위, 중복 여부 및 달성 가능성(선행조건)을 검증하는 컴포넌트입니다."""

    @classmethod
    def validate(
        cls,
        draft: SupportQuestDraft,
        context: QuestContext,
        active_target_ids: set[str],
    ) -> ValidationResult:
        """지원 퀘스트 초안에 대한 통합 검증을 가동합니다.

        Args:
            draft: 검증 대상 지원 퀘스트 초안
            context: 정규화된 공장 상태 컨텍스트
            active_target_ids: 이미 활성화된 지원 퀘스트들의 목표 자원 ID 집합

        Returns:
            검증 성공 여부와 실패 원인을 담은 ValidationResult 객체
        """
        # 0. 기본 형태 및 구성 검증
        if not draft.objectives:
            return ValidationResult(
                valid=False,
                reason="invalid_objectives",
                message="퀘스트 목표가 존재하지 않습니다.",
            )

        valid_item_ids = game_data.get_valid_item_ids()
        recipe_map = game_data.get_recipe_map()

        for obj in draft.objectives:
            # 1. 자원 ID 실존 검증
            if obj.target_id not in valid_item_ids:
                return ValidationResult(
                    valid=False,
                    reason="invalid_item",
                    message=f"존재하지 않는 자원 ID 입니다: {obj.target_id}",
                )

            # 2. 목표 유형 검증
            if obj.type != "collect_item":
                return ValidationResult(
                    valid=False,
                    reason="invalid_type",
                    message=f"지원하지 않는 퀘스트 목표 유형입니다: {obj.type}",
                )

            # 3. 목표 수량 검증 (0 초과, 1000 이하)
            if obj.target_amount <= 0 or obj.target_amount > 1000:
                return ValidationResult(
                    valid=False,
                    reason="invalid_amount",
                    message=f"목표 수량이 허용 범위를 벗어났습니다 (1~1000): {obj.target_amount}",
                )

            # 4. 동일 자원 active 중복 검증
            if obj.target_id in active_target_ids:
                return ValidationResult(
                    valid=False,
                    reason="duplicate_support_quest",
                    message=f"동일한 자원을 요구하는 지원 퀘스트가 이미 활성화 상태입니다: {obj.target_id}",
                )

            # 5. 선행조건 검증 (달성 가능성 검증)
            if not cls._check_prerequisite(obj.target_id, context, recipe_map):
                return ValidationResult(
                    valid=False,
                    reason="unreachable_prerequisite",
                    message=f"해당 자원({obj.target_id})을 획득할 수 있는 생산 레시피가 해금되지 않았거나 입력 재료가 부족합니다.",
                )

        return ValidationResult(valid=True)

    @classmethod
    def _check_prerequisite(
        cls,
        target_id: str,
        context: QuestContext,
        recipe_map: dict[str, tuple[str, list[str]]],
    ) -> bool:
        """해당 자원을 획득 가능한 선행조건이 충족되는지 판단합니다.

        재귀적 DFS 탐색을 통해 2단계 이상의 제작 체인 및 원재료 수급 상태를 종합 평가합니다.
        """
        return cls._can_produce(target_id, context, recipe_map, set())

    @classmethod
    def _can_produce(
        cls,
        target_id: str,
        context: QuestContext,
        recipe_map: dict[str, tuple[str, list[str]]],
        visited: set[str],
    ) -> bool:
        """재귀적으로 입력 자원 생산 및 보유 가능 여부를 탐색하는 내부 검사 로직입니다.

        Args:
            target_id: 검사할 자원 ID
            context: 정규화된 공장 상태 컨텍스트
            recipe_map: 레시피 맵 정보
            visited: 순환 참조 방지를 위한 방문 노드 집합
        """
        # 1. 이미 인벤토리에 존재하는 경우 충족
        if context.inventory.get(target_id, 0) > 0:
            return True

        # 순환 참조 방지
        if target_id in visited:
            return False
        visited.add(target_id)

        # 해당 자원을 생성하는 레시피 수집
        producing_recipes = []
        for r_id, (out_item, in_items) in recipe_map.items():
            if out_item == target_id:
                producing_recipes.append((r_id, in_items))

        # 2. 레시피가 없는 자원은 원재료(채굴/채집 대상)이므로 획득 가능하다고 판정
        if not producing_recipes:
            return True

        # 3. 해금된 레시피 중 하나라도 하위 입력 자원들이 재귀적으로 모두 확보 가능하다면 참
        for r_id, in_items in producing_recipes:
            norm_r_id = r_id if r_id.startswith("recipe_") else f"recipe_{r_id}"
            if norm_r_id not in context.unlocked_recipes:
                continue

            inputs_satisfied = True
            for in_item in in_items:
                # 하위 재료에 대해 재귀 검사 수행
                if not cls._can_produce(in_item, context, recipe_map, visited.copy()):
                    inputs_satisfied = False
                    break

            if inputs_satisfied:
                return True

        return False
