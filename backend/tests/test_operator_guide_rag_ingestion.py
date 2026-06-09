"""Tests for preparing Manual Q&A RAG documents for embedding ingestion."""

from __future__ import annotations

from agents.operator_guide.rag_documents import ManualRagDocument
from agents.operator_guide.rag_ingestion import (
    EmbeddingProvider,
    ManualRagIngestionService,
)


class FakeEmbeddingProvider:
    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        return [[float(len(text)), 1.0] for text in texts]


def test_ingestion_record_keeps_source_metadata_and_embedding_text() -> None:
    document = ManualRagDocument(
        doc_id="equipment:equipment_smelter",
        source_file="equipment.csv",
        source_row_id="equipment_smelter",
        title="제련기",
        content="장비: 제련기\n역할: 광석을 제련합니다.",
        metadata={"record_type": "equipment"},
    )

    records = ManualRagIngestionService(FakeEmbeddingProvider()).build_records(
        [document],
    )

    assert len(records) == 1
    record = records[0]
    assert record.doc_id == "equipment:equipment_smelter"
    assert record.source_file == "equipment.csv"
    assert record.source_row_id == "equipment_smelter"
    assert record.title == "제련기"
    assert record.embedding_text == document.content
    assert record.metadata == {"record_type": "equipment"}
    assert record.embedding == [float(len(document.content)), 1.0]


def test_ingestion_record_content_hash_is_stable() -> None:
    document = ManualRagDocument(
        doc_id="troubleshooting:issue_machine_stopped",
        source_file="troubleshooting_rules.csv",
        source_row_id="issue_machine_stopped",
        title="장비가 멈췄을 때",
        content="문제: 장비가 멈췄을 때\n해결: 전력과 입력 자원을 확인합니다.",
        metadata={"record_type": "troubleshooting"},
    )
    service = ManualRagIngestionService(FakeEmbeddingProvider())

    first = service.build_records([document])[0]
    second = service.build_records([document])[0]

    assert first.content_hash == second.content_hash
    assert len(first.content_hash) == 64


def test_embedding_provider_contract_is_structural() -> None:
    provider: EmbeddingProvider = FakeEmbeddingProvider()

    assert provider.embed_texts(["abc"]) == [[3.0, 1.0]]
