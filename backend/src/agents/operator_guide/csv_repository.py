"""CSV repository for the Manual Q&A proto."""

from __future__ import annotations

import csv
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


def _project_root() -> Path:
    return Path(__file__).resolve().parents[4]


def _split_ids(value: str) -> list[str]:
    if not value or value in {"none", "any"}:
        return []
    return [item.strip() for item in value.split(";") if item.strip()]


def _strip_quantity(value: str) -> str:
    return value.split(":", 1)[0]


class CsvManualQARepository:
    """Read exactly the five proto CSV files under data/game."""

    def __init__(self, data_dir: Path | None = None) -> None:
        self._data_dir = data_dir or (_project_root() / "data" / "game")

    @cached_property
    def _equipment(self) -> dict[str, EquipmentRecord]:
        rows = self._read_rows("equipment.csv")
        return {
            row["equipment_id"]: EquipmentRecord(
                equipment_id=row["equipment_id"],
                name=row["name"],
                category=row["category"],
                role=row["role"],
                input_resources=_split_ids(row["input_resources"]),
                output_resources=_split_ids(row["output_resources"]),
                power_required=row["power_required"],
                connectable_equipment=_split_ids(row["connectable_equipment"]),
                related_recipes=_split_ids(row["related_recipes"]),
                common_issues=_split_ids(row["common_issues"]),
            )
            for row in rows
        }

    @cached_property
    def _resources(self) -> dict[str, ResourceRecord]:
        rows = self._read_rows("resources.csv")
        return {
            row["resource_id"]: ResourceRecord(
                resource_id=row["resource_id"],
                name=row["name"],
                kind=row["kind"],
                acquisition_method=row["acquisition_method"],
                produced_by=row["produced_by"],
                used_for=row["used_for"],
                used_in_recipes=_split_ids(row["used_in_recipes"]),
                related_resources=_split_ids(row["related_resources"]),
            )
            for row in rows
        }

    @cached_property
    def _recipes(self) -> dict[str, RecipeRecord]:
        rows = self._read_rows("recipes.csv")
        return {
            row["recipe_id"]: RecipeRecord(
                recipe_id=row["recipe_id"],
                name=row["name"],
                input_resources=_split_ids(row["input_resources"]),
                output_resource=_strip_quantity(row["output_resource"]),
                required_equipment=row["required_equipment"],
                stage=row["stage"],
                prerequisite_recipes=_split_ids(row["prerequisite_recipes"]),
                production_steps=row["production_steps"],
                common_bottlenecks=_split_ids(row["common_bottlenecks"]),
            )
            for row in rows
        }

    @cached_property
    def _troubleshooting_rules(self) -> dict[str, TroubleshootingRuleRecord]:
        rows = self._read_rows("troubleshooting_rules.csv")
        return {
            row["issue_id"]: TroubleshootingRuleRecord(
                issue_id=row["issue_id"],
                name=row["name"],
                symptom=_split_ids(row["symptom"]),
                possible_causes=_split_ids(row["possible_causes"]),
                check_order=_split_ids(row["check_order"]),
                recommended_action_ids=_split_ids(row["recommended_action_ids"]),
                resolution=row["resolution"],
                related_equipment=_split_ids(row["related_equipment"]),
                related_resources=_split_ids(row["related_resources"]),
            )
            for row in rows
        }

    @cached_property
    def _action_policies(self) -> dict[str, ActionPolicyRecord]:
        rows = self._read_rows("action_policy.csv")
        return {
            row["action_id"]: ActionPolicyRecord(
                action_id=row["action_id"],
                label=row["label"],
                description=row["description"],
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
            if rule.name in question or any(symptom in question for symptom in rule.symptom):
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
