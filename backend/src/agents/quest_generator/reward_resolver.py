"""QuestRewardResolver for checking and filtering quest rewards to avoid infinite loops."""

from __future__ import annotations

from agents.quest_generator import game_data
from agents.quest_generator.models import QuestReward


class QuestRewardResolver:
    """퀘스트 보상의 정합성을 검증하고, 특정 아이템에 의존적인 순환 파밍 방지를 위한 필터링을 담당합니다."""

    @classmethod
    def resolve_rewards(
        cls, rewards: list[QuestReward], target_item_id: str
    ) -> list[QuestReward]:
        """퀘스트 보상 리스트를 정제하여 순환 파밍 가능성이 있는 아이템 보상을 제외합니다.

        Args:
            rewards: 정제할 원본 보상 목록
            target_item_id: 해당 퀘스트의 목표 자원 ID

        Returns:
            필터링 완료된 안전한 보상 목록
        """
        if not target_item_id:
            return rewards

        recipe_map = game_data.get_recipe_map()

        # 제외할 자원 목록을 DFS로 재귀 수집 (목표 자원 자체 포함)
        excluded_item_ids = {target_item_id}
        cls._collect_all_inputs(target_item_id, recipe_map, excluded_item_ids, set())

        resolved_rewards = []
        for reward in rewards:
            # 보상의 target_id가 제외 대상 자원에 속하면 필터링
            if reward.target_id in excluded_item_ids:
                continue
            resolved_rewards.append(reward)

        return resolved_rewards

    @classmethod
    def _collect_all_inputs(
        cls,
        item_id: str,
        recipe_map: dict[str, tuple[str, list[str]]],
        excluded: set[str],
        visited: set[str],
    ) -> None:
        """아이템 생산 레시피 상의 모든 하위 입력/중간재 자원을 재귀적으로 수집합니다."""
        if item_id in visited:
            return
        visited.add(item_id)

        for _, (out_item, in_items) in recipe_map.items():
            if out_item == item_id:
                for in_item in in_items:
                    excluded.add(in_item)
                    cls._collect_all_inputs(
                        in_item, recipe_map, excluded, visited.copy()
                    )
