"""Plan and execute Manual Q&A RAG document upserts."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from agents.operator_guide.rag_ingestion import (
    ManualRagIngestionBatch,
    ManualRagIngestionRecord,
)


class ManualRagDocumentStore(Protocol):
    """Storage contract for Manual Q&A RAG documents."""

    def list_active_content_hashes(self) -> dict[str, str]:
        """Return active document content hashes by document id."""

    def upsert_records(self, records: list[ManualRagIngestionRecord]) -> None:
        """Insert or update RAG ingestion records."""

    def deactivate_missing(self, doc_ids: list[str]) -> None:
        """Mark documents inactive when they no longer exist in CSV."""


@dataclass(frozen=True)
class ManualRagUpsertSummary:
    """Summary of a Manual Q&A RAG ingestion run."""

    inserted: int
    updated: int
    skipped: int
    deactivated: int
    failed: int = 0


class ManualRagUpsertService:
    """Compare new ingestion records with storage and apply minimal changes."""

    def __init__(self, store: ManualRagDocumentStore) -> None:
        self._store = store

    def upsert(
        self,
        records: list[ManualRagIngestionRecord],
        *,
        dry_run: bool = False,
        force: bool = False,
    ) -> ManualRagUpsertSummary:
        return self.upsert_batch(
            ManualRagIngestionBatch(
                records=records,
                content_hashes={record.doc_id: record.content_hash for record in records},
            ),
            dry_run=dry_run,
            force=force,
        )

    def upsert_batch(
        self,
        batch: ManualRagIngestionBatch,
        *,
        dry_run: bool = False,
        force: bool = False,
    ) -> ManualRagUpsertSummary:
        existing_hashes = self._store.list_active_content_hashes()
        incoming_ids = set(batch.content_hashes)
        records_by_id = {record.doc_id: record for record in batch.records}
        records_to_write: list[ManualRagIngestionRecord] = []
        inserted = 0
        updated = 0
        skipped = 0

        for doc_id, content_hash in batch.content_hashes.items():
            existing_hash = existing_hashes.get(doc_id)
            if existing_hash is None:
                inserted += 1
                if doc_id in records_by_id:
                    records_to_write.append(records_by_id[doc_id])
            elif force or existing_hash != content_hash:
                updated += 1
                if doc_id in records_by_id:
                    records_to_write.append(records_by_id[doc_id])
            else:
                skipped += 1

        missing_doc_ids = sorted(set(existing_hashes) - incoming_ids)
        summary = ManualRagUpsertSummary(
            inserted=inserted,
            updated=updated,
            skipped=skipped,
            deactivated=len(missing_doc_ids),
        )

        if dry_run:
            return summary

        if records_to_write:
            self._store.upsert_records(records_to_write)
        if missing_doc_ids:
            self._store.deactivate_missing(missing_doc_ids)
        return summary
