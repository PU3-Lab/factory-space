"""Tests for Manual Q&A RAG upsert planning and execution."""

from __future__ import annotations

from agents.operator_guide.rag_ingestion import ManualRagIngestionRecord
from agents.operator_guide.rag_upsert import ManualRagUpsertService


class FakeManualRagStore:
    def __init__(self, hashes: dict[str, str]) -> None:
        self.hashes = hashes
        self.upserted: list[ManualRagIngestionRecord] = []
        self.deactivated: list[str] = []

    def list_active_content_hashes(self) -> dict[str, str]:
        return self.hashes

    def upsert_records(self, records: list[ManualRagIngestionRecord]) -> None:
        self.upserted.extend(records)

    def deactivate_missing(self, doc_ids: list[str]) -> None:
        self.deactivated.extend(doc_ids)


def test_dry_run_reports_changes_without_writing_to_store() -> None:
    unchanged = _record("doc:same", "same-hash")
    new = _record("doc:new", "new-hash")
    store = FakeManualRagStore({"doc:same": "same-hash"})

    summary = ManualRagUpsertService(store).upsert(
        [unchanged, new],
        dry_run=True,
    )

    assert summary.inserted == 1
    assert summary.updated == 0
    assert summary.skipped == 1
    assert summary.deactivated == 0
    assert summary.failed == 0
    assert store.upserted == []
    assert store.deactivated == []


def test_matching_content_hash_is_skipped() -> None:
    record = _record("doc:same", "same-hash")
    store = FakeManualRagStore({"doc:same": "same-hash"})

    summary = ManualRagUpsertService(store).upsert([record])

    assert summary.skipped == 1
    assert store.upserted == []


def test_changed_content_hash_is_updated() -> None:
    record = _record("doc:changed", "new-hash")
    store = FakeManualRagStore({"doc:changed": "old-hash"})

    summary = ManualRagUpsertService(store).upsert([record])

    assert summary.updated == 1
    assert store.upserted == [record]


def test_force_updates_matching_content_hash() -> None:
    record = _record("doc:same", "same-hash")
    store = FakeManualRagStore({"doc:same": "same-hash"})

    summary = ManualRagUpsertService(store).upsert([record], force=True)

    assert summary.updated == 1
    assert summary.skipped == 0
    assert store.upserted == [record]


def test_missing_existing_document_is_deactivated() -> None:
    store = FakeManualRagStore({"doc:removed": "old-hash"})

    summary = ManualRagUpsertService(store).upsert([])

    assert summary.deactivated == 1
    assert store.deactivated == ["doc:removed"]


def _record(doc_id: str, content_hash: str) -> ManualRagIngestionRecord:
    return ManualRagIngestionRecord(
        doc_id=doc_id,
        source_file="equipment.csv",
        source_row_id=doc_id,
        title=doc_id,
        embedding_text=f"content for {doc_id}",
        content_hash=content_hash,
        metadata={"record_type": "equipment"},
        embedding=[1.0, 2.0],
    )
