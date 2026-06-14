"""Build CSV evidence context for operator guide prompts."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

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


@dataclass(frozen=True)
class ManualQAPromptContext:
    """CSV evidence and metadata used to prompt the LLM."""

    result: ManualQAResult
    evidence: dict[str, Any]


class ManualQAContextBuilder:
    """Convert classified manual intent into compact CSV evidence."""

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def build(self, question: str, intent: ManualQAIntent) -> ManualQAPromptContext:
        sources: list[ManualQASource] = []
        recommended_action_ids: list[str] = []
        evidence: dict[str, Any] = {
            "question_type": intent.question_type,
            "primary_manual": intent.primary_manual,
            "supporting_manuals": intent.supporting_manuals,
            "target_ids": intent.target_ids,
            "records": [],
        }

        for target_id in intent.target_ids:
            if (equipment := self._repository.get_equipment(target_id)) is not None:
                sources.append(self._equipment_source(equipment))
                evidence["records"].append(self._equipment_evidence(equipment))
                continue
            if (resource := self._repository.get_resource(target_id)) is not None:
                sources.append(self._resource_source(resource))
                evidence["records"].append(self._resource_evidence(resource))
                continue
            if (recipe := self._repository.get_recipe(target_id)) is not None:
                sources.append(self._recipe_source(recipe))
                evidence["records"].append(self._recipe_evidence(recipe))
                for raw_resource_id in recipe.input_resources:
                    resource_id = raw_resource_id.split(":", 1)[0]
                    resource = self._repository.get_resource(resource_id)
                    if resource is not None:
                        sources.append(self._resource_source(resource))
                        evidence["records"].append(self._resource_evidence(resource))
                continue
            if (
                rule := self._repository.get_troubleshooting_rule(target_id)
            ) is not None:
                sources.append(self._rule_source(rule))
                recommended_action_ids.extend(rule.recommended_action_ids)
                evidence["records"].append(self._rule_evidence(rule))

        if not recommended_action_ids:
            recommended_action_ids = self._default_action_ids(intent.question_type)

        actions = self._recommended_actions(recommended_action_ids)
        evidence["recommended_actions"] = [action.model_dump() for action in actions]

        result = ManualQAResult(
            question=question,
            question_type=intent.question_type,
            answer=self._fallback_answer(has_evidence=bool(sources)),
            sources=self._dedupe_sources(sources),
            recommended_actions=actions,
            confidence=self._confidence(intent, sources),
            primary_manual=intent.primary_manual,
            supporting_manuals=intent.supporting_manuals,
            target_ids=intent.target_ids,
        )
        return ManualQAPromptContext(result=result, evidence=evidence)

    def _recommended_actions(
        self,
        action_ids: list[str],
    ) -> list[RecommendedAction]:
        return [
            self._recommended_action(policy, priority)
            for priority, policy in enumerate(
                self._repository.get_action_policies(action_ids),
                start=1,
            )
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

    def _default_action_ids(self, question_type: str) -> list[str]:
        if question_type == "equipment_question":
            return ["action_explain_equipment_role"]
        if question_type == "resource_question":
            return ["action_explain_resource_production"]
        if question_type == "recipe_question":
            return ["action_explain_recipe_requirements"]
        if question_type == "unknown_question":
            return ["action_answer_unknown_without_guessing"]
        return []

    def _confidence(
        self,
        intent: ManualQAIntent,
        sources: list[ManualQASource],
    ) -> str:
        if intent.question_type == "unknown_question":
            return "low"
        if sources:
            return "high"
        return "low"

    def _fallback_answer(self, *, has_evidence: bool) -> str:
        if has_evidence:
            return "LLM answer unavailable. Please check the matched manual evidence."
        return "LLM answer unavailable. No matching manual data was found."

    def _dedupe_sources(
        self,
        sources: list[ManualQASource],
    ) -> list[ManualQASource]:
        seen: set[str] = set()
        output: list[ManualQASource] = []
        for source in sources:
            if source.doc_id in seen:
                continue
            seen.add(source.doc_id)
            output.append(source)
        return output

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

    def _equipment_evidence(self, equipment: EquipmentRecord) -> dict[str, Any]:
        return {
            "type": "equipment",
            "equipment_id": equipment.equipment_id,
            "name": equipment.name,
            "category": equipment.category,
            "role": equipment.role,
            "input_resources": equipment.input_resources,
            "output_resources": equipment.output_resources,
            "power_required": equipment.power_required,
            "connectable_equipment": equipment.connectable_equipment,
            "related_recipes": equipment.related_recipes,
            "common_issues": equipment.common_issues,
        }

    def _resource_evidence(self, resource: ResourceRecord) -> dict[str, Any]:
        return {
            "type": "resource",
            "resource_id": resource.resource_id,
            "name": resource.name,
            "kind": resource.kind,
            "acquisition_method": resource.acquisition_method,
            "produced_by": resource.produced_by,
            "used_for": resource.used_for,
            "used_in_recipes": resource.used_in_recipes,
            "related_resources": resource.related_resources,
        }

    def _recipe_evidence(self, recipe: RecipeRecord) -> dict[str, Any]:
        return {
            "type": "recipe",
            "recipe_id": recipe.recipe_id,
            "name": recipe.name,
            "input_resources": recipe.input_resources,
            "output_resource": recipe.output_resource,
            "required_equipment": recipe.required_equipment,
            "stage": recipe.stage,
            "prerequisite_recipes": recipe.prerequisite_recipes,
            "production_steps": recipe.production_steps,
            "common_bottlenecks": recipe.common_bottlenecks,
        }

    def _rule_evidence(self, rule: TroubleshootingRuleRecord) -> dict[str, Any]:
        return {
            "type": "troubleshooting",
            "issue_id": rule.issue_id,
            "name": rule.name,
            "symptom": rule.symptom,
            "possible_causes": rule.possible_causes,
            "check_order": rule.check_order,
            "recommended_action_ids": rule.recommended_action_ids,
            "resolution": rule.resolution,
            "related_equipment": rule.related_equipment,
            "related_resources": rule.related_resources,
        }
