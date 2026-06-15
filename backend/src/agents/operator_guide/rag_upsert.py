"""RAG 문서를 DB에 얼마나 insert/update/skip할지 계산하고 실행하는 모듈.

초보자용 설명:
    CSV가 자주 바뀌어도 모든 문서를 매번 다시 embedding하면 비용이 든다.
    이 파일은 content hash를 비교해서 새 문서, 바뀐 문서, 그대로인 문서를 나눈다.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from agents.operator_guide.rag_ingestion import (
    ManualRagIngestionBatch,
    ManualRagIngestionRecord,
)


class ManualRagDocumentStore(Protocol):
    """RAG upsert 서비스가 기대하는 저장소 인터페이스."""

    def list_active_content_hashes(self) -> dict[str, str]:
        """활성 문서의 content hash를 doc_id 기준으로 반환한다."""

    def upsert_records(self, records: list[ManualRagIngestionRecord]) -> None:
        """RAG ingestion record를 insert 또는 update한다."""

    def deactivate_missing(self, doc_ids: list[str]) -> None:
        """CSV에서 사라진 문서를 inactive로 표시한다."""


@dataclass(frozen=True)
class ManualRagUpsertSummary:
    """ingestion 실행 결과 요약."""

    inserted: int
    updated: int
    skipped: int
    deactivated: int
    failed: int = 0


class ManualRagUpsertService:
    """기존 DB 상태와 새 batch를 비교해서 필요한 변경만 적용한다."""

    def __init__(self, store: ManualRagDocumentStore) -> None:
        self._store = store

    def upsert(
        self,
        records: list[ManualRagIngestionRecord],
        *,
        dry_run: bool = False,
        force: bool = False,
    ) -> ManualRagUpsertSummary:
        """record 목록을 받아 바로 upsert batch로 처리한다."""

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
        """insert/update/skip/deactivate 수를 계산하고, dry-run이 아니면 DB에 반영한다."""

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
