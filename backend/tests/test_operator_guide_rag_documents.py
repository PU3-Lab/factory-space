"""Tests for converting Manual Q&A CSV rows into RAG documents."""

from __future__ import annotations

from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.rag_documents import ManualRagDocumentBuilder


def test_rag_documents_include_all_manual_csv_rows() -> None:
    documents = ManualRagDocumentBuilder(CsvManualQARepository()).build_all()

    doc_ids = {document.doc_id for document in documents}

    assert "equipment:equipment_smelter" in doc_ids
    assert "resource:resource_iron_ingot" in doc_ids
    assert "recipe:recipe_smelt_iron" in doc_ids
    assert "troubleshooting:issue_machine_stopped" in doc_ids
    assert "action:action_check_power" in doc_ids
    assert "tutorial:TUT_BASIC_001" in doc_ids


def test_equipment_rag_document_preserves_searchable_csv_context() -> None:
    documents = ManualRagDocumentBuilder(CsvManualQARepository()).build_all()
    smelter = next(
        document
        for document in documents
        if document.doc_id == "equipment:equipment_smelter"
    )

    assert smelter.source_file == "equipment.csv"
    assert smelter.source_row_id == "equipment_smelter"
    assert smelter.title == "제련기"
    assert smelter.metadata["record_type"] == "equipment"
    assert "장비: 제련기" in smelter.content
    assert "입력 자원:" in smelter.content
    assert "출력 자원:" in smelter.content
    assert "전력 요구량: 10" in smelter.content
    assert "관련 문제:" in smelter.content


def test_troubleshooting_rag_document_preserves_actions_and_resolution() -> None:
    documents = ManualRagDocumentBuilder(CsvManualQARepository()).build_all()
    rule = next(
        document
        for document in documents
        if document.doc_id == "troubleshooting:issue_machine_stopped"
    )

    assert rule.source_file == "troubleshooting_rules.csv"
    assert rule.source_row_id == "issue_machine_stopped"
    assert rule.metadata["record_type"] == "troubleshooting"
    assert "문제: 장비가 멈췄을 때" in rule.content
    assert "증상:" in rule.content
    assert "확인 순서:" in rule.content
    assert "추천 액션:" in rule.content
    assert "해결:" in rule.content


def test_tutorial_rag_document_preserves_progress_context() -> None:
    documents = ManualRagDocumentBuilder(CsvManualQARepository()).build_all()
    tutorial = next(
        document
        for document in documents
        if document.doc_id == "tutorial:TUT_BASIC_001"
    )

    assert tutorial.source_file == "tutorial.csv"
    assert tutorial.source_row_id == "TUT_BASIC_001"
    assert tutorial.title == "이동하기"
    assert tutorial.metadata["record_type"] == "tutorial"
    assert tutorial.metadata["group_id"] == "tutorial_basic"
    assert "튜토리얼: 이동하기" in tutorial.content
    assert "그룹: 기초 조작 (tutorial_basic)" in tutorial.content
    assert "다음 튜토리얼: TUT_BASIC_002" in tutorial.content
