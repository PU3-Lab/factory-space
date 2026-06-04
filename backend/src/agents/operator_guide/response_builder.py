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
            f"좋아요. {equipment.name}는 {equipment.role}입니다. "
            f"{self._equipment_input_phrase(equipment.input_resources)} "
            f"{self._equipment_output_phrase(equipment.output_resources)} "
            f"사용할 때는 전력 요구량 {equipment.power_required}과 "
            "컨베이어 연결 상태를 먼저 확인해보세요."
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
            f"좋아요. {resource.name}는 "
            f"{self._formal_statement(resource.acquisition_method)}. "
            f"보통 {self._usage_phrase(resource.used_for)} "
            f"생산 흐름은 {equipment.name if equipment else resource.produced_by}에서 "
            "먼저 확인해보면 됩니다."
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

        output_name = output_resource.name if output_resource else recipe.name
        answer = (
            f"좋아요. {self._object_phrase(output_name)} 만들려면 "
            f"{self._resource_amounts(recipe.input_resources)} 필요합니다. "
            f"{equipment.name if equipment else recipe.required_equipment}에서 제작하고, "
            f"흐름은 {recipe.production_steps} 순서로 보면 됩니다. "
            "먼저 필요한 재료가 장비까지 들어오는지 확인해볼까요?"
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
        input_hint = (
            self._resource_hint(equipment.input_resources, "입력 자원")
            if equipment
            else "입력 자원"
        )
        output_hint = (
            self._resource_hint(equipment.output_resources, "출력 자원")
            if equipment
            else "출력 자원"
        )
        answer = (
            f"{target_name}가 멈췄군요. 보통 전력, 입력 자원, 출력 이동 중 "
            "하나가 막히면 생산이 멈춥니다. "
            f"먼저 전력이 제대로 들어오는지 확인해보세요. "
            f"전력이 괜찮다면 {input_hint}이 {target_name} 안으로 들어오고 있는지 살펴보고, "
            f"만들어진 {output_hint}이 컨베이어나 저장고로 빠져나갈 수 있는지도 확인하면 됩니다. "
            f"마지막으로 {target_name}에 올바른 레시피가 선택되어 있는지 확인하세요."
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
                "그 질문은 현재 매뉴얼 데이터에서 확인할 수 없습니다. "
                "저는 장비, 자원, 레시피, 생산 문제 해결을 도와드릴 수 있어요. "
                "예를 들면 '제련기가 왜 안 돌아가?', "
                "'기어 만들려면 뭐가 필요해?'처럼 물어보면 안내해드릴 수 있습니다."
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
            description=self._formal_statement(policy.description),
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
            if item_id == "none":
                names.append("별도 입력 자원 없음")
                continue
            if item_id == "any":
                names.append("여러 자원")
                continue
            resource = self._repository.get_resource(item_id)
            equipment = self._repository.get_equipment(item_id)
            names.append(
                (resource and resource.name) or (equipment and equipment.name) or raw_id
            )
        return ", ".join(names) if names else "없음"

    def _equipment_input_phrase(self, ids: list[str]) -> str:
        if ids == ["none"]:
            return "입력 자원은 따로 필요하지 않습니다."
        if ids == ["any"]:
            return "입력 쪽에서는 여러 자원을 받을 수 있습니다."
        return f"입력 자원은 {self._names(ids)}입니다."

    def _equipment_output_phrase(self, ids: list[str]) -> str:
        if ids == ["none"]:
            return "출력 자원은 따로 없습니다."
        if ids == ["any"]:
            return "출력 쪽에서는 여러 자원을 내보낼 수 있습니다."
        return f"출력 결과는 {self._names(ids)}입니다."

    def _resource_amounts(self, ids: list[str]) -> str:
        amounts = []
        for raw_id in ids:
            item_id, _, quantity = raw_id.partition(":")
            resource = self._repository.get_resource(item_id)
            name = (resource and resource.name) or item_id
            amounts.append(f"{name} {quantity}개가" if quantity else f"{name}이")
        return ", ".join(amounts) if amounts else "없음이"

    def _resource_hint(self, ids: list[str], fallback: str) -> str:
        names = self._names(ids)
        if names == "없음":
            return fallback
        return f"{names} 같은 {fallback}"

    def _formal_statement(self, text: str) -> str:
        if text.endswith("한다"):
            return f"{text[:-2]}합니다"
        return text

    def _object_phrase(self, text: str) -> str:
        return f"{text}{'을' if self._has_final_consonant(text) else '를'}"

    def _has_final_consonant(self, text: str) -> bool:
        if not text:
            return False
        code = ord(text[-1])
        if code < 0xAC00 or code > 0xD7A3:
            return False
        return (code - 0xAC00) % 28 != 0

    def _usage_phrase(self, text: str) -> str:
        if text.endswith("에 사용"):
            return f"{text}됩니다."
        return f"{text}입니다."

