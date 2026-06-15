"""Convert Manual Q&A CSV rows into RAG-ready documents."""

from __future__ import annotations

from dataclasses import dataclass

from agents.operator_guide.csv_repository import (
    ActionPolicyRecord,
    CsvManualQARepository,
    EquipmentRecord,
    RecipeRecord,
    ResourceRecord,
    TroubleshootingRuleRecord,
)


@dataclass(frozen=True)
class ManualRagDocument:
    """One normalized manual row ready for embedding and vector storage."""

    doc_id: str
    source_file: str
    source_row_id: str
    title: str
    content: str
    metadata: dict[str, str]


class ManualRagDocumentBuilder:
    """Build RAG documents from the current Manual Q&A CSV repository."""

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def build_all(self) -> list[ManualRagDocument]:
        """Return one RAG document per source CSV row."""

        documents: list[ManualRagDocument] = []
        documents.extend(
            self._equipment_document(record)
            for record in self._repository.list_equipment()
        )
        documents.extend(
            self._resource_document(record)
            for record in self._repository.list_resources()
        )
        documents.extend(
            self._recipe_document(record) for record in self._repository.list_recipes()
        )
        documents.extend(
            self._troubleshooting_document(record)
            for record in self._repository.list_troubleshooting_rules()
        )
        documents.extend(
            self._action_document(record)
            for record in self._repository.list_action_policies()
        )
        return documents

    def _equipment_document(self, record: EquipmentRecord) -> ManualRagDocument:
        return ManualRagDocument(
            doc_id=f"equipment:{record.equipment_id}",
            source_file="equipment.csv",
            source_row_id=record.equipment_id,
            title=record.name,
            content="\n".join(
                [
                    f"장비: {record.name}",
                    f"분류: {record.category}",
                    f"역할: {record.role}",
                    f"입력 자원: {_join_values(record.input_resources)}",
                    f"출력 자원: {_join_values(record.output_resources)}",
                    f"전력 요구량: {record.power_required}",
                    f"연결 가능 장비: {_join_values(record.connectable_equipment)}",
                    f"관련 레시피: {_join_values(record.related_recipes)}",
                    f"관련 문제: {_join_values(record.common_issues)}",
                ],
            ),
            metadata={
                "record_type": "equipment",
                "category": record.category,
            },
        )

    def _resource_document(self, record: ResourceRecord) -> ManualRagDocument:
        return ManualRagDocument(
            doc_id=f"resource:{record.resource_id}",
            source_file="resources.csv",
            source_row_id=record.resource_id,
            title=record.name,
            content="\n".join(
                [
                    f"자원: {record.name}",
                    f"종류: {record.kind}",
                    f"획득 방법: {record.acquisition_method}",
                    f"생산 장비: {record.produced_by}",
                    f"사용처: {record.used_for}",
                    f"사용 레시피: {_join_values(record.used_in_recipes)}",
                    f"관련 자원: {_join_values(record.related_resources)}",
                ],
            ),
            metadata={
                "record_type": "resource",
                "kind": record.kind,
            },
        )

    def _recipe_document(self, record: RecipeRecord) -> ManualRagDocument:
        return ManualRagDocument(
            doc_id=f"recipe:{record.recipe_id}",
            source_file="recipes.csv",
            source_row_id=record.recipe_id,
            title=record.name,
            content="\n".join(
                [
                    f"레시피: {record.name}",
                    f"입력 자원: {_join_values(record.input_resources)}",
                    f"출력 자원: {record.output_resource}",
                    f"필요 장비: {record.required_equipment}",
                    f"단계: {record.stage}",
                    f"선행 레시피: {_join_values(record.prerequisite_recipes)}",
                    f"생산 절차: {record.production_steps}",
                    f"주요 병목: {_join_values(record.common_bottlenecks)}",
                ],
            ),
            metadata={
                "record_type": "recipe",
                "stage": record.stage,
            },
        )

    def _troubleshooting_document(
        self,
        record: TroubleshootingRuleRecord,
    ) -> ManualRagDocument:
        return ManualRagDocument(
            doc_id=f"troubleshooting:{record.issue_id}",
            source_file="troubleshooting_rules.csv",
            source_row_id=record.issue_id,
            title=record.name,
            content="\n".join(
                [
                    f"문제: {record.name}",
                    f"증상: {_join_values(record.symptom)}",
                    f"가능 원인: {_join_values(record.possible_causes)}",
                    f"확인 순서: {_join_values(record.check_order)}",
                    f"추천 액션: {_join_values(record.recommended_action_ids)}",
                    f"해결: {record.resolution}",
                    f"관련 장비: {_join_values(record.related_equipment)}",
                    f"관련 자원: {_join_values(record.related_resources)}",
                ],
            ),
            metadata={
                "record_type": "troubleshooting",
            },
        )

    def _action_document(self, record: ActionPolicyRecord) -> ManualRagDocument:
        return ManualRagDocument(
            doc_id=f"action:{record.action_id}",
            source_file="action_policy.csv",
            source_row_id=record.action_id,
            title=record.label,
            content="\n".join(
                [
                    f"액션: {record.label}",
                    f"설명: {record.description}",
                ],
            ),
            metadata={
                "record_type": "action",
            },
        )


def _join_values(values: list[str]) -> str:
    return ", ".join(values) if values else "없음"
