"""Build Manual Q&A proto answers from repository records."""

from __future__ import annotations

from agents.operator_guide.csv_repository import (
    ActionPolicyRecord,
    CsvManualQARepository,
    EquipmentRecord,
    RecipeRecord,
    ResourceRecord,
    TroubleshootingRuleRecord,
)
from agents.operator_guide.schemas import (
    ManualQAIntent,
    ManualQAResult,
    ManualQASource,
    RecommendedAction,
)


class ManualQAResponseBuilder:
    """Create text, sources, and recommendation metadata."""

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def build(self, question: str, intent: ManualQAIntent) -> ManualQAResult:
        if intent.question_type == "equipment_question":
            return self._build_equipment_answer(question, intent)
        if intent.question_type == "resource_question":
            return self._build_resource_answer(question, intent)
        if intent.question_type == "recipe_question":
            return self._build_recipe_answer(question, intent)
        if intent.question_type == "troubleshooting_question":
            return self._build_troubleshooting_answer(question, intent)
        return self._build_unknown_answer(question, intent)

    def _build_equipment_answer(
        self,
        question: str,
        intent: ManualQAIntent,
    ) -> ManualQAResult:
        equipment = self._first_equipment(intent)
        if equipment is None:
            return self._build_unknown_answer(question, intent)

        answer = (
            f"{equipment.name}는 {equipment.role}입니다. "
            f"입력 자원은 {self._names(equipment.input_resources)}이고, "
            f"출력 자원은 {self._names(equipment.output_resources)}입니다. "
            f"필요 전력은 {equipment.power_required}입니다."
        )
        return ManualQAResult(
            question=question,
            question_type="equipment_question",
            answer=answer,
            sources=[self._equipment_source(equipment)],
            recommended_actions=self._recommended_actions(
                ["action_explain_equipment_role"]
            ),
            confidence="high",
            primary_manual=intent.primary_manual,
            supporting_manuals=intent.supporting_manuals,
            target_ids=intent.target_ids,
        )

    def _build_resource_answer(
        self,
        question: str,
        intent: ManualQAIntent,
    ) -> ManualQAResult:
        resource = self._first_resource(intent)
        if resource is None:
            return self._build_unknown_answer(question, intent)

        recipe = self._repository.find_recipe_by_output_resource(resource.resource_id)
        equipment = self._repository.get_equipment(resource.produced_by)
        sources = [self._resource_source(resource)]
        if recipe is not None:
            sources.append(self._recipe_source(recipe))

        answer = (
            f"{resource.name}는 {resource.acquisition_method}. "
            f"생산 장비는 {equipment.name if equipment else resource.produced_by}이고, "
            f"사용처는 {resource.used_for}입니다."
        )
        if recipe is not None:
            answer += f" 관련 공정은 {recipe.name}입니다."

        return ManualQAResult(
            question=question,
            question_type="resource_question",
            answer=answer,
            sources=sources,
            recommended_actions=self._recommended_actions(
                ["action_explain_resource_production"]
            ),
            confidence="high",
            primary_manual=intent.primary_manual,
            supporting_manuals=intent.supporting_manuals,
            target_ids=intent.target_ids,
        )

    def _build_recipe_answer(
        self,
        question: str,
        intent: ManualQAIntent,
    ) -> ManualQAResult:
        recipe = self._first_recipe(intent)
        if recipe is None:
            return self._build_unknown_answer(question, intent)

        output_resource = self._repository.get_resource(recipe.output_resource)
        equipment = self._repository.get_equipment(recipe.required_equipment)
        input_ids = [item.split(":", 1)[0] for item in recipe.input_resources]
        sources = [self._recipe_source(recipe)]
        sources.extend(
            self._resource_source(resource)
            for resource_id in input_ids
            if (resource := self._repository.get_resource(resource_id)) is not None
        )

        answer = (
            f"{output_resource.name if output_resource else recipe.name} 제작에는 "
            f"{self._names(recipe.input_resources)}이 필요합니다. "
            f"필요 장비는 {equipment.name if equipment else recipe.required_equipment}이고, "
            f"공정 순서는 {recipe.production_steps}입니다."
        )

        return ManualQAResult(
            question=question,
            question_type="recipe_question",
            answer=answer,
            sources=sources,
            recommended_actions=self._recommended_actions(
                ["action_explain_recipe_requirements"]
            ),
            confidence="high",
            primary_manual=intent.primary_manual,
            supporting_manuals=intent.supporting_manuals,
            target_ids=intent.target_ids,
        )

    def _build_troubleshooting_answer(
        self,
        question: str,
        intent: ManualQAIntent,
    ) -> ManualQAResult:
        equipment = self._first_equipment(intent)
        rule = self._first_rule(intent)
        if rule is None:
            return self._build_unknown_answer(question, intent)

        sources = [self._rule_source(rule)]
        if equipment is not None:
            sources.append(self._equipment_source(equipment))

        target_name = equipment.name if equipment is not None else "장비"
        answer = (
            f"{target_name}가 멈췄다면 {rule.resolution}. "
            f"확인 순서는 {', '.join(rule.check_order)}입니다."
        )

        return ManualQAResult(
            question=question,
            question_type="troubleshooting_question",
            answer=answer,
            sources=sources,
            recommended_actions=self._recommended_actions(
                rule.recommended_action_ids
            ),
            confidence="medium",
            primary_manual=intent.primary_manual,
            supporting_manuals=intent.supporting_manuals,
            target_ids=intent.target_ids,
        )

    def _build_unknown_answer(
        self,
        question: str,
        intent: ManualQAIntent,
    ) -> ManualQAResult:
        return ManualQAResult(
            question=question,
            question_type="unknown_question",
            answer=(
                "현재 매뉴얼 데이터에서 확인할 수 없습니다. "
                "프로토는 장비, 자원, 레시피, 문제 해결 CSV에 있는 내용만 답변합니다."
            ),
            sources=[],
            recommended_actions=self._recommended_actions(
                ["action_answer_unknown_without_guessing"]
            ),
            confidence="low",
            primary_manual="unknown",
            supporting_manuals=[],
            target_ids=[],
        )

    def _first_equipment(self, intent: ManualQAIntent) -> EquipmentRecord | None:
        for target_id in intent.target_ids:
            if (equipment := self._repository.get_equipment(target_id)) is not None:
                return equipment
        return None

    def _first_resource(self, intent: ManualQAIntent) -> ResourceRecord | None:
        for target_id in intent.target_ids:
            if (resource := self._repository.get_resource(target_id)) is not None:
                return resource
        return None

    def _first_recipe(self, intent: ManualQAIntent) -> RecipeRecord | None:
        for target_id in intent.target_ids:
            if (recipe := self._repository.get_recipe(target_id)) is not None:
                return recipe
        return None

    def _first_rule(self, intent: ManualQAIntent) -> TroubleshootingRuleRecord | None:
        for target_id in intent.target_ids:
            if (rule := self._repository.get_troubleshooting_rule(target_id)) is not None:
                return rule
        return None

    def _recommended_actions(
        self,
        action_ids: list[str],
    ) -> list[RecommendedAction]:
        policies = self._repository.get_action_policies(action_ids)
        return [
            self._recommended_action(policy, priority)
            for priority, policy in enumerate(policies, start=1)
        ]

    def _recommended_action(
        self,
        policy: ActionPolicyRecord,
        priority: int,
    ) -> RecommendedAction:
        return RecommendedAction(
            action_id=policy.action_id,
            label=policy.label,
            description=policy.description,
            priority=priority,
        )

    def _equipment_source(self, equipment: EquipmentRecord) -> ManualQASource:
        return ManualQASource(
            doc_id=equipment.equipment_id,
            type="equipment",
            title=equipment.name,
        )

    def _resource_source(self, resource: ResourceRecord) -> ManualQASource:
        return ManualQASource(
            doc_id=resource.resource_id,
            type="resource",
            title=resource.name,
        )

    def _recipe_source(self, recipe: RecipeRecord) -> ManualQASource:
        return ManualQASource(
            doc_id=recipe.recipe_id,
            type="recipe",
            title=recipe.name,
        )

    def _rule_source(self, rule: TroubleshootingRuleRecord) -> ManualQASource:
        return ManualQASource(
            doc_id=rule.issue_id,
            type="troubleshooting",
            title=rule.name,
        )

    def _names(self, ids: list[str]) -> str:
        names = []
        for raw_id in ids:
            item_id = raw_id.split(":", 1)[0]
            resource = self._repository.get_resource(item_id)
            equipment = self._repository.get_equipment(item_id)
            names.append((resource and resource.name) or (equipment and equipment.name) or raw_id)
        return ", ".join(names) if names else "없음"

