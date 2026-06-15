"""데이터베이스로부터 시스템 레시피를 쿼리하고 매칭하기 위한 레포지토리입니다."""

from __future__ import annotations

from typing import Any

from sqlalchemy import select
from sqlalchemy.orm import Session

from db.models import RecipeModel


class RecipeRepository:
    """캐시된 알려진 아이템들을 관리하고 작성된 레시피들의 매칭을 처리합니다."""

    @classmethod
    def reload_cache(cls, session: Session) -> None:
        """DB에서 모든 레시피를 로드하고 세션 메모리 캐시를 구성합니다."""
        result = session.execute(select(RecipeModel))
        recipes = list(result.scalars().all())
        session.info["recipes_cache"] = recipes

        items = set()
        for r in recipes:
            for item in (
                r.input_item_1,
                r.input_item_2,
                r.input_item_3,
                r.output_item_1,
                r.output_item_2,
            ):
                if item:
                    items.add(item)
        session.info["known_items"] = items

    @classmethod
    def get_known_items(cls, session: Session) -> set[str]:
        """기존 레시피에 존재하는 모든 아이템 ID 세트를 반환합니다."""
        if "known_items" not in session.info:
            cls.reload_cache(session)
        return session.info["known_items"]

    @classmethod
    def get_recipes(cls, session: Session) -> list[RecipeModel]:
        """모든 레시피 목록을 반환합니다."""
        if "recipes_cache" not in session.info:
            cls.reload_cache(session)
        return session.info["recipes_cache"]

    @classmethod
    def match_recipe(
        cls,
        session: Session,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
    ) -> RecipeModel | None:
        """주어진 장비 및 입력 아이템/수량과 완전히 일치하는 레시피를 찾습니다."""
        recipes = cls.get_recipes(session)
        input_map = {item["item_id"]: item["qty"] for item in normalized_inputs}

        for r in recipes:
            if r.machine_type != machine_type:
                continue

            recipe_inputs = {}
            if r.input_item_1:
                recipe_inputs[r.input_item_1] = r.input_qty_1
            if r.input_item_2:
                recipe_inputs[r.input_item_2] = r.input_qty_2
            if r.input_item_3:
                recipe_inputs[r.input_item_3] = r.input_qty_3

            if recipe_inputs == input_map:
                return r

        return None
