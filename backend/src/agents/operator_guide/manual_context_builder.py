"""Build CSV evidence context for operator guide prompts."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
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

_RAW_ID_PAREN_RE = re.compile(
    r"\s*\((?:equipment|resource|recipe|issue|action)_[^)]+\)"
)


@dataclass(frozen=True)
class ManualQAPromptContext:
    """LLM 프롬프트를 구성하는 데 사용되는 CSV 매뉴얼 근거, RAG 검색 결과 및 게임 상태 정보 등을 담는 데이터 컨텍스트입니다.

    초보자용 설명:
        이 클래스는 에이전트의 여러 로직(CSV 검색, RAG 검색, 실시간 게임 상태 조회)을 거쳐 수집된
        모든 텍스트 정보와 메타데이터를 통합하여 프롬프트 빌더(Prompt Builder)에 전달하기 위한 바구니 역할을 합니다.
    """

    result: ManualQAResult
    evidence: dict[str, Any]
    rag_context_text: str = ""
    rag_metadata: dict[str, Any] | None = None
    recent_conversation: list[dict[str, str]] = field(default_factory=list)
    confirmed_facts: list[str] = field(default_factory=list)
    current_game_state_text: str = ""
    requires_current_game_state: bool = False
    used_current_game_state: bool = False
    required_state_scopes: list[str] = field(default_factory=list)
    available_scopes: list[str] = field(default_factory=list)
    response_style: str = "normal"


class ManualQAContextBuilder:
    """분류된 질문 의도(Intent) 데이터를 바탕으로 이에 대응하는 CSV 매뉴얼 상세 레코드를 조회하여 컨텍스트로 변환하는 클래스입니다.

    초보자용 설명:
        플레이어의 질문 분류(예: recipe_question) 결과에 매칭되는 장비, 자원, 레시피 등의 상세 지식을 데이터베이스에서 찾아 담아줍니다.
    """

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def build(self, question: str, intent: ManualQAIntent) -> ManualQAPromptContext:
        """질문 텍스트와 분류 인텐트를 입력받아 매칭되는 매뉴얼 원본 데이터 및 추천 동작들을 묶은 컨텍스트 객체를 생성합니다.

        입력값:
            - question (str): 플레이어 질문 원문
            - intent (ManualQAIntent): 분류된 질문의 의도 및 타겟 ID가 담긴 인텐트 객체

        반환값:
            - ManualQAPromptContext: 매뉴얼 지식 및 추천 동작이 조립된 LLM 입력용 데이터 컨텍스트
        """
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
            answer=self._fallback_answer(
                has_evidence=bool(sources),
                records=evidence["records"],
            ),
            sources=self._dedupe_sources(sources),
            recommended_actions=actions,
            confidence=self._confidence(intent, sources),
            primary_manual=intent.primary_manual,
            supporting_manuals=intent.supporting_manuals,
            target_ids=intent.target_ids,
            is_ambiguous=intent.is_ambiguous,
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

    def _fallback_answer(
        self,
        *,
        has_evidence: bool,
        records: list[dict[str, Any]] | None = None,
    ) -> str:
        if has_evidence and records:
            if rule_answer := self._rule_fallback_answer(records):
                return rule_answer
            if recipe_answer := self._recipe_fallback_answer(records):
                return recipe_answer
            if resource_answer := self._resource_fallback_answer(records):
                return resource_answer
            if equipment_answer := self._equipment_fallback_answer(records):
                return equipment_answer

        if has_evidence:
            return (
                "지금은 답변을 완성하지 못했지만, 관련 매뉴얼 근거는 찾았습니다.\n\n"
                "잠시 후 다시 물어보면 확인한 내용을 바탕으로 정리해드릴게요."
            )
        return (
            "지금은 관련 매뉴얼 근거를 찾지 못했습니다.\n\n"
            "장비 이름이나 자원 이름을 조금 더 구체적으로 말해주면 다시 확인해볼게요."
        )

    def _recipe_fallback_answer(self, records: list[dict[str, Any]]) -> str:
        recipe = self._first_record(records, "recipe")
        if recipe is None:
            return ""

        output_name = self._resource_display_name(str(recipe.get("output_resource") or ""))
        recipe_name = self._display_text(str(recipe.get("name") or ""))
        target_name = output_name or recipe_name or "이 자원"
        equipment_name = self._equipment_display_name(
            str(recipe.get("required_equipment") or "")
        )
        input_resources = [
            self._resource_amount_display_name(str(raw_resource))
            for raw_resource in recipe.get("input_resources", [])
        ]
        input_resources = [resource for resource in input_resources if resource]
        first_sentence = self._subject(target_name)
        if equipment_name:
            first_sentence += f" {equipment_name}에서 만들 수 있어요."
        else:
            first_sentence += " 관련 제작 공정에서 만들 수 있어요."

        detail_parts: list[str] = []
        if input_resources:
            detail_parts.append(f"필요 재료는 {', '.join(input_resources)}입니다.")
        production_steps = self._display_text(str(recipe.get("production_steps") or ""))
        if production_steps:
            detail_parts.append(f"생산 흐름은 {production_steps} 순서입니다.")
        if not detail_parts:
            detail_parts.append("세부 재료와 생산 장비는 매뉴얼의 해당 제작 공정을 확인하면 됩니다.")

        return f"{first_sentence}\n\n{' '.join(detail_parts)}"

    def _resource_fallback_answer(self, records: list[dict[str, Any]]) -> str:
        resource = self._first_record(records, "resource")
        if resource is None:
            return ""

        name = self._display_text(str(resource.get("name") or "이 자원"))
        acquisition = self._display_text(str(resource.get("acquisition_method") or ""))
        produced_by = self._equipment_display_name(str(resource.get("produced_by") or ""))
        if acquisition:
            first_sentence = acquisition
        elif produced_by:
            first_sentence = f"{self._subject(name)} {produced_by}에서 생산합니다."
        else:
            first_sentence = f"{name}에 대한 매뉴얼 근거를 찾았습니다."

        used_for = self._display_text(str(resource.get("used_for") or ""))
        if used_for:
            return f"{first_sentence}\n\n주요 사용처는 {used_for}입니다."
        return f"{first_sentence}\n\n관련 제작 공정과 연결 장비를 함께 확인해보면 좋아요."

    def _equipment_fallback_answer(self, records: list[dict[str, Any]]) -> str:
        equipment = self._first_record(records, "equipment")
        if equipment is None:
            return ""

        name = self._display_text(str(equipment.get("name") or "이 장비"))
        role = self._display_text(str(equipment.get("role") or ""))
        output_resources = [
            self._resource_display_name(str(raw_resource))
            for raw_resource in equipment.get("output_resources", [])
        ]
        output_resources = [resource for resource in output_resources if resource]
        if role:
            first_sentence = f"{self._subject(name)} {role}입니다."
        else:
            first_sentence = f"{name}에 대한 매뉴얼 근거를 찾았습니다."

        if output_resources:
            visible_outputs = output_resources[:3]
            output_text = ", ".join(visible_outputs)
            if len(output_resources) > len(visible_outputs):
                output_text += " 등"
            return f"{first_sentence}\n\n대표 출력 자원은 {output_text}입니다."
        return f"{first_sentence}\n\n입력과 출력 연결 상태를 함께 확인해보면 좋아요."

    def _rule_fallback_answer(self, records: list[dict[str, Any]]) -> str:
        rule = self._first_record(records, "troubleshooting")
        if rule is None:
            return ""

        name = self._display_text(str(rule.get("name") or "문제 상황"))
        resolution = self._display_text(str(rule.get("resolution") or ""))
        check_order = [
            self._display_text(str(raw_step))
            for raw_step in rule.get("check_order", [])
        ]
        check_order = [step for step in check_order if step]

        first_sentence = f"{name}는 먼저 원인을 순서대로 좁혀보는 게 좋아요."
        if resolution:
            first_sentence = resolution

        if check_order:
            if len(check_order) == 1:
                return f"{first_sentence}\n\n먼저 {check_order[0]}하세요."
            return f"{first_sentence}\n\n확인 순서는 {', '.join(check_order)}입니다."
        return f"{first_sentence}\n\n전력, 입력 자원, 출력 공간부터 차례로 확인해보세요."

    def _first_record(
        self,
        records: list[dict[str, Any]],
        record_type: str,
    ) -> dict[str, Any] | None:
        for record in records:
            if record.get("type") == record_type:
                return record
        return None

    def _resource_amount_display_name(self, value: str) -> str:
        resource_id, _, amount = value.partition(":")
        name = self._resource_display_name(resource_id)
        if not name:
            return ""
        if amount:
            return f"{name} {amount}개"
        return name

    def _resource_display_name(self, value: str) -> str:
        resource_id = value.partition(":")[0]
        resource = self._repository.get_resource(resource_id)
        if resource is not None:
            return resource.name
        return self._display_text(value)

    def _equipment_display_name(self, value: str) -> str:
        equipment_id = value.partition(":")[0]
        equipment = self._repository.get_equipment(equipment_id)
        if equipment is not None:
            return equipment.name
        return self._display_text(value)

    def _display_text(self, value: str) -> str:
        return _RAW_ID_PAREN_RE.sub("", value).strip()

    def _subject(self, name: str) -> str:
        if not name:
            return "이 항목은"
        last_char = name[-1]
        if "가" <= last_char <= "힣" and (ord(last_char) - ord("가")) % 28:
            return f"{name}은"
        return f"{name}는"

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
