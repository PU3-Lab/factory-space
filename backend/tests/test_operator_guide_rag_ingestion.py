"""Tests for preparing Manual Q&A RAG documents for embedding ingestion."""

from __future__ import annotations

from agents.operator_guide.rag_documents import ManualRagDocument
from agents.operator_guide.rag_ingestion import (
    EmbeddingProvider,
    ManualRagIngestionService,
)


class FakeEmbeddingProvider:
    def __init__(self) -> None:
        self.calls: list[list[str]] = []

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        self.calls.append(texts)
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


def test_ingestion_batch_embeds_only_changed_documents() -> None:
    unchanged = ManualRagDocument(
        doc_id="equipment:unchanged",
        source_file="equipment.csv",
        source_row_id="unchanged",
        title="변경 없음",
        content="장비: 변경 없음",
        metadata={"record_type": "equipment"},
    )
    changed = ManualRagDocument(
        doc_id="equipment:changed",
        source_file="equipment.csv",
        source_row_id="changed",
        title="변경됨",
        content="장비: 변경됨",
        metadata={"record_type": "equipment"},
    )
    provider = FakeEmbeddingProvider()
    service = ManualRagIngestionService(provider)
    unchanged_hash = service.build_records([unchanged])[0].content_hash

    batch = service.build_batch(
        [unchanged, changed],
        existing_content_hashes={"equipment:unchanged": unchanged_hash},
    )

    assert [record.doc_id for record in batch.records] == ["equipment:changed"]
    assert provider.calls[-1] == [changed.content]
    assert set(batch.content_hashes) == {"equipment:unchanged", "equipment:changed"}


def test_dry_run_ingestion_batch_does_not_call_embedding_provider() -> None:
    document = ManualRagDocument(
        doc_id="equipment:new",
        source_file="equipment.csv",
        source_row_id="new",
        title="신규",
        content="장비: 신규",
        metadata={"record_type": "equipment"},
    )
    provider = FakeEmbeddingProvider()

    batch = ManualRagIngestionService(provider).build_batch(
        [document],
        existing_content_hashes={},
        dry_run=True,
    )

    assert batch.records == []
    assert list(batch.content_hashes) == ["equipment:new"]
    assert provider.calls == []


def test_embedding_provider_contract_is_structural() -> None:
    provider: EmbeddingProvider = FakeEmbeddingProvider()

    assert provider.embed_texts(["abc"]) == [[3.0, 1.0]]
