"""CSV repository for the Manual Q&A proto."""

from __future__ import annotations

import csv
import re
from collections.abc import Iterable
from dataclasses import dataclass
from functools import cached_property
from pathlib import Path


@dataclass(frozen=True)
class EquipmentRecord:
    equipment_id: str
    name: str
    category: str
    role: str
    input_resources: list[str]
    output_resources: list[str]
    power_required: str
    connectable_equipment: list[str]
    related_recipes: list[str]
    common_issues: list[str]


@dataclass(frozen=True)
class ResourceRecord:
    resource_id: str
    name: str
    kind: str
    acquisition_method: str
    produced_by: str
    used_for: str
    used_in_recipes: list[str]
    related_resources: list[str]


@dataclass(frozen=True)
class RecipeRecord:
    recipe_id: str
    name: str
    input_resources: list[str]
    output_resource: str
    required_equipment: str
    stage: str
    prerequisite_recipes: list[str]
    production_steps: str
    common_bottlenecks: list[str]


@dataclass(frozen=True)
class TroubleshootingRuleRecord:
    issue_id: str
    name: str
    symptom: list[str]
    possible_causes: list[str]
    check_order: list[str]
    recommended_action_ids: list[str]
    resolution: str
    related_equipment: list[str]
    related_resources: list[str]


@dataclass(frozen=True)
class ActionPolicyRecord:
    action_id: str
    label: str
    description: str


@dataclass(frozen=True)
class TutorialRecord:
    """튜토리얼 CSV 한 줄을 코드에서 다루기 쉽게 담는 값 객체.

    `tutorial.csv`는 플레이어에게 어떤 순서로 안내를 보여줄지와 NPC 대사를 담는다.
    RAG에서는 이 정보를 검색 가능한 문서로 바꾸어, "다음에 뭘 해야 해?" 같은
    진행 질문에 근거로 사용할 수 있다.
    """

    tutorial_id: str
    next_tutorial_id: str
    group_id: str
    group_name: str
    title: str
    description: str
    start_dialogue: str
    complete_dialogue: str
    failure_dialogue: str
    related_equipment: list[str]
    related_resources: list[str]
    related_recipes: list[str]


def _project_root() -> Path:
    return Path(__file__).resolve().parents[4]


def _split_ids(value: str) -> list[str]:
    if not value or value in {"none", "any", "없음"}:
        return []
    return [_normalize_id_token(item) for item in value.split(";") if item.strip()]


def _strip_quantity(value: str) -> str:
    return _normalize_id_token(value).split(":", 1)[0]


def _normalize_id_token(value: str) -> str:
    stripped = value.strip()
    id_match = re.search(r"\(([^()]+)\)", stripped)
    if id_match is None:
        return stripped
    suffix = ""
    tail = stripped[id_match.end() :].strip()
    if tail.startswith(":"):
        suffix = tail
    return f"{id_match.group(1)}{suffix}"


def _value(row: dict[str, str], *keys: str) -> str:
    for key in keys:
        if key in row:
            return row[key]
    return ""


class CsvManualQARepository:
    """operator_guide가 참고할 CSV 파일을 읽는 저장소.

    초보자용 설명:
        이 클래스는 `data/game` 폴더의 CSV를 Python 객체로 바꿔준다.
        RAG ingestion은 여기서 읽은 장비, 자원, 레시피, 문제 해결, 액션 정책,
        튜토리얼 데이터를 다시 검색 가능한 문서로 변환한다.
    """

    def __init__(self, data_dir: Path | None = None) -> None:
        self._data_dir = data_dir or (_project_root() / "data" / "game")

    @cached_property
    def _equipment(self) -> dict[str, EquipmentRecord]:
        rows = self._read_rows("equipment.csv")
        return {
            _value(row, "equipment_id", "장비ID"): EquipmentRecord(
                equipment_id=_value(row, "equipment_id", "장비ID"),
                name=_value(row, "name", "장비명"),
                category=_value(row, "category", "분류"),
                role=_value(row, "role", "역할"),
                input_resources=_split_ids(_value(row, "input_resources", "입력자원")),
                output_resources=_split_ids(
                    _value(row, "output_resources", "출력자원")
                ),
                power_required=_value(row, "power_required", "필요전력"),
                connectable_equipment=_split_ids(
                    _value(row, "connectable_equipment", "연결가능장비")
                ),
                related_recipes=_split_ids(
                    _value(row, "related_recipes", "관련레시피")
                ),
                common_issues=_split_ids(_value(row, "common_issues", "자주발생문제")),
            )
            for row in rows
        }

    @cached_property
    def _resources(self) -> dict[str, ResourceRecord]:
        rows = self._read_rows("resources.csv")
        return {
            _value(row, "resource_id", "자원ID"): ResourceRecord(
                resource_id=_value(row, "resource_id", "자원ID"),
                name=_value(row, "name", "자원명"),
                kind=_value(row, "kind", "종류"),
                acquisition_method=_value(row, "acquisition_method", "획득방법"),
                produced_by=_value(row, "produced_by", "생산장비"),
                used_for=_value(row, "used_for", "사용처"),
                used_in_recipes=_split_ids(
                    _value(row, "used_in_recipes", "사용레시피")
                ),
                related_resources=_split_ids(
                    _value(row, "related_resources", "관련자원")
                ),
            )
            for row in rows
        }

    @cached_property
    def _recipes(self) -> dict[str, RecipeRecord]:
        rows = self._read_rows("recipes.csv")
        return {
            _value(row, "recipe_id", "레시피ID"): RecipeRecord(
                recipe_id=_value(row, "recipe_id", "레시피ID"),
                name=_value(row, "name", "레시피명"),
                input_resources=_split_ids(_value(row, "input_resources", "입력자원")),
                output_resource=_strip_quantity(
                    _value(row, "output_resource", "출력자원")
                ),
                required_equipment=_strip_quantity(
                    _value(row, "required_equipment", "필요장비")
                ),
                stage=_value(row, "stage", "공정단계"),
                prerequisite_recipes=_split_ids(
                    _value(row, "prerequisite_recipes", "선행레시피")
                ),
                production_steps=_value(row, "production_steps", "생산순서"),
                common_bottlenecks=_split_ids(
                    _value(row, "common_bottlenecks", "자주발생문제")
                ),
            )
            for row in rows
        }

    @cached_property
    def _troubleshooting_rules(self) -> dict[str, TroubleshootingRuleRecord]:
        rows = self._read_rows("troubleshooting_rules.csv")
        return {
            _value(row, "issue_id", "문제ID"): TroubleshootingRuleRecord(
                issue_id=_value(row, "issue_id", "문제ID"),
                name=_value(row, "name", "문제명"),
                symptom=_split_ids(_value(row, "symptom", "증상")),
                possible_causes=_split_ids(_value(row, "possible_causes", "가능원인")),
                check_order=_split_ids(_value(row, "check_order", "확인순서")),
                recommended_action_ids=_split_ids(
                    _value(row, "recommended_action_ids", "추천행동ID")
                ),
                resolution=_value(row, "resolution", "해결방법"),
                related_equipment=_split_ids(
                    _value(row, "related_equipment", "관련장비")
                ),
                related_resources=_split_ids(
                    _value(row, "related_resources", "관련자원")
                ),
            )
            for row in rows
        }

    @cached_property
    def _action_policies(self) -> dict[str, ActionPolicyRecord]:
        rows = self._read_rows("action_policy.csv")
        return {
            _value(row, "action_id", "행동ID"): ActionPolicyRecord(
                action_id=_value(row, "action_id", "행동ID"),
                label=_value(row, "label", "행동명"),
                description=_value(row, "description", "설명"),
            )
            for row in rows
        }

    @cached_property
    def _tutorials(self) -> dict[str, TutorialRecord]:
        rows = self._read_rows("tutorial.csv")
        return {
            _value(row, "tutorial_id", "튜토리얼ID"): TutorialRecord(
                tutorial_id=_value(row, "tutorial_id", "튜토리얼ID"),
                next_tutorial_id=_value(row, "next_tutorial_id", "다음튜토리얼ID"),
                group_id=_value(row, "group_id", "그룹ID"),
                group_name=_value(row, "group_name", "그룹명"),
                title=_value(row, "title", "제목"),
                description=_value(row, "description", "설명"),
                start_dialogue=_value(row, "start_dialogue", "시작대사"),
                complete_dialogue=_value(row, "complete_dialogue", "완료대사"),
                failure_dialogue=_value(row, "failure_dialogue", "실패대사"),
                related_equipment=_split_ids(
                    _value(row, "related_equipment", "관련장비")
                ),
                related_resources=_split_ids(
                    _value(row, "related_resources", "관련자원")
                ),
                related_recipes=_split_ids(_value(row, "related_recipes", "관련레시피")),
            )
            for row in rows
        }

    def get_equipment(self, equipment_id: str) -> EquipmentRecord | None:
        return self._equipment.get(equipment_id)

    def list_equipment(self) -> list[EquipmentRecord]:
        return list(self._equipment.values())

    def find_equipment_by_question(self, question: str) -> EquipmentRecord | None:
        return self._find_by_name(question, self._equipment.values())

    def get_resource(self, resource_id: str) -> ResourceRecord | None:
        return self._resources.get(resource_id)

    def list_resources(self) -> list[ResourceRecord]:
        return list(self._resources.values())

    def find_resource_by_question(self, question: str) -> ResourceRecord | None:
        return self._find_by_name(question, self._resources.values())

    def get_recipe(self, recipe_id: str) -> RecipeRecord | None:
        return self._recipes.get(recipe_id)

    def list_recipes(self) -> list[RecipeRecord]:
        return list(self._recipes.values())

    def find_recipe_by_question(self, question: str) -> RecipeRecord | None:
        resource = self.find_resource_by_question(question)
        if resource is not None:
            recipe = self.find_recipe_by_output_resource(resource.resource_id)
            if recipe is not None:
                return recipe
        return self._find_by_name(question, self._recipes.values())

    def find_recipe_by_output_resource(self, resource_id: str) -> RecipeRecord | None:
        for recipe in self._recipes.values():
            if recipe.output_resource == resource_id:
                return recipe
        return None

    def get_troubleshooting_rule(
        self,
        issue_id: str,
    ) -> TroubleshootingRuleRecord | None:
        return self._troubleshooting_rules.get(issue_id)

    def list_troubleshooting_rules(self) -> list[TroubleshootingRuleRecord]:
        return list(self._troubleshooting_rules.values())

    def find_troubleshooting_rule(
        self,
        question: str,
        equipment_id: str | None = None,
    ) -> TroubleshootingRuleRecord | None:
        if equipment_id is not None:
            machine_stopped = self.get_troubleshooting_rule("issue_machine_stopped")
            if (
                machine_stopped is not None
                and equipment_id in machine_stopped.related_equipment
            ):
                return machine_stopped

        for rule in self._troubleshooting_rules.values():
            if rule.name in question or any(
                symptom in question for symptom in rule.symptom
            ):
                return rule
        return None

    def get_action_policy(self, action_id: str) -> ActionPolicyRecord | None:
        return self._action_policies.get(action_id)

    def list_action_policies(self) -> list[ActionPolicyRecord]:
        return list(self._action_policies.values())

    def get_action_policies(self, action_ids: list[str]) -> list[ActionPolicyRecord]:
        return [
            policy
            for action_id in action_ids
            if (policy := self.get_action_policy(action_id)) is not None
        ]

    def get_tutorial(self, tutorial_id: str) -> TutorialRecord | None:
        """튜토리얼 ID로 특정 튜토리얼 row를 찾는다."""

        return self._tutorials.get(tutorial_id)

    def list_tutorials(self) -> list[TutorialRecord]:
        """RAG 문서로 변환할 모든 튜토리얼 row를 반환한다."""

        return list(self._tutorials.values())

    def _read_rows(self, filename: str) -> list[dict[str, str]]:
        path = self._data_dir / filename
        with path.open(encoding="utf-8-sig", newline="") as csv_file:
            return list(csv.DictReader(csv_file))

    def _find_by_name[T](self, question: str, records: Iterable[T]) -> T | None:
        for record in records:
            name = getattr(record, "name")
            if name and name in question:
                return record
        return None
