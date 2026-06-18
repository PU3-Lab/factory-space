"""CSV 한 줄을 RAG 검색에 넣기 좋은 문서 형태로 바꾸는 모듈.

초보자용 설명:
    RAG는 질문과 비슷한 문서를 먼저 찾고, 그 문서를 근거로 LLM이 답하는 방식이다.
    이 파일은 장비/자원/레시피 같은 CSV row를 검색 가능한 텍스트 문서로 정리한다.
"""

from __future__ import annotations

from dataclasses import dataclass

from agents.operator_guide.csv_repository import (
    ActionPolicyRecord,
    CsvManualQARepository,
    EquipmentRecord,
    RecipeRecord,
    ResourceRecord,
    TroubleshootingRuleRecord,
    TutorialRecord,
)


@dataclass(frozen=True)
class ManualRagDocument:
    """RAG에 저장할 문서 한 개.

    CSV row 하나를 사람이 읽기 쉬운 `content`로 만들고, 나중에 어디서 온
    문서인지 추적할 수 있게 `source_file`, `source_row_id`, `metadata`를 함께 들고 간다.
    """

    doc_id: str
    source_file: str
    source_row_id: str
    title: str
    content: str
    metadata: dict[str, str]


class ManualRagDocumentBuilder:
    """CSV repository에서 모든 매뉴얼 row를 읽어 RAG 문서 목록으로 만든다."""

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def build_all(self) -> list[ManualRagDocument]:
        """현재 CSV의 주요 row를 모두 RAG 문서로 변환한다."""

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
        documents.extend(
            self._tutorial_document(record)
            for record in self._repository.list_tutorials()
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

    def _tutorial_document(self, record: TutorialRecord) -> ManualRagDocument:
        """튜토리얼 row를 RAG 검색용 문서로 변환한다.

        초보자용 설명:
            튜토리얼 CSV는 "플레이어에게 어떤 안내를 보여줄지"를 담고 있다.
            이 함수는 그 한 줄을 검색 가능한 텍스트로 풀어 써서,
            LLM이 진행 방향 질문에 답할 때 근거로 사용할 수 있게 만든다.
        """

        return ManualRagDocument(
            doc_id=f"tutorial:{record.tutorial_id}",
            source_file="tutorial.csv",
            source_row_id=record.tutorial_id,
            title=record.title,
            content="\n".join(
                [
                    f"튜토리얼: {record.title}",
                    f"그룹: {record.group_name} ({record.group_id})",
                    f"설명: {record.description}",
                    f"시작 대사: {record.start_dialogue}",
                    f"완료 대사: {record.complete_dialogue}",
                    f"실패 대사: {record.failure_dialogue or '없음'}",
                    f"다음 튜토리얼: {record.next_tutorial_id or '없음'}",
                    f"관련 장비: {_join_values(record.related_equipment)}",
                    f"관련 자원: {_join_values(record.related_resources)}",
                    f"관련 레시피: {_join_values(record.related_recipes)}",
                ],
            ),
            metadata={
                "record_type": "tutorial",
                "group_id": record.group_id,
                "group_name": record.group_name,
            },
        )


def _join_values(values: list[str]) -> str:
    return ", ".join(values) if values else "없음"
