"""Rule-based question classification for the Manual Q&A proto."""

from __future__ import annotations

from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.schemas import ManualQAIntent

TROUBLESHOOTING_KEYWORDS = (
    "왜",
    "안 돌아",
    "안돌아",
    "작동하지",
    "멈",
    "고장",
    "부족",
    "막",
)
RECIPE_KEYWORDS = ("필요", "만들려면", "재료", "요구")
RESOURCE_PRODUCTION_KEYWORDS = ("어떻게 만들어", "어떻게 만들", "생산", "제작")
EQUIPMENT_KEYWORDS = ("뭐야", "무엇", "역할", "설명")


class ManualQAQuestionClassifier:
    """Classify a question using simple proto-stage rules."""

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def classify(self, question: str) -> ManualQAIntent:
        equipment = self._repository.find_equipment_by_question(question)
        resource = self._repository.find_resource_by_question(question)
        recipe = self._repository.find_recipe_by_question(question)

        if self._has_any(question, TROUBLESHOOTING_KEYWORDS):
            target_ids = []
            if equipment is not None:
                target_ids.append(equipment.equipment_id)
            target_ids.append("issue_machine_stopped")
            return ManualQAIntent(
                question_type="troubleshooting_question",
                primary_manual="troubleshooting",
                supporting_manuals=[
                    "equipment",
                    "resources",
                    "recipes",
                    "action_policy",
                ],
                target_ids=target_ids,
            )

        if resource is not None and self._has_any(question, RECIPE_KEYWORDS):
            recipe_for_resource = self._repository.find_recipe_by_output_resource(
                resource.resource_id
            )
            if recipe_for_resource is not None:
                return ManualQAIntent(
                    question_type="recipe_question",
                    primary_manual="recipes",
                    supporting_manuals=["resources", "equipment"],
                    target_ids=[recipe_for_resource.recipe_id, resource.resource_id],
                )

        if resource is not None and self._has_any(
            question, RESOURCE_PRODUCTION_KEYWORDS
        ):
            target_ids = [resource.resource_id]
            recipe_for_resource = self._repository.find_recipe_by_output_resource(
                resource.resource_id
            )
            if recipe_for_resource is not None:
                target_ids.append(recipe_for_resource.recipe_id)
            return ManualQAIntent(
                question_type="resource_question",
                primary_manual="resources",
                supporting_manuals=["recipes", "equipment"],
                target_ids=target_ids,
            )

        if recipe is not None and self._has_any(question, RECIPE_KEYWORDS):
            return ManualQAIntent(
                question_type="recipe_question",
                primary_manual="recipes",
                supporting_manuals=["resources", "equipment"],
                target_ids=[recipe.recipe_id, *_recipe_resource_ids(recipe)],
            )

        if equipment is not None and self._has_any(question, EQUIPMENT_KEYWORDS):
            return ManualQAIntent(
                question_type="equipment_question",
                primary_manual="equipment",
                supporting_manuals=["recipes", "troubleshooting"],
                target_ids=[equipment.equipment_id],
            )

        return ManualQAIntent(
            question_type="unknown_question",
            primary_manual="unknown",
            supporting_manuals=[],
            target_ids=[],
        )

    def _has_any(self, question: str, keywords: tuple[str, ...]) -> bool:
        return any(keyword in question for keyword in keywords)


def _recipe_resource_ids(recipe: object) -> list[str]:
    return [item.split(":", 1)[0] for item in getattr(recipe, "input_resources")]
