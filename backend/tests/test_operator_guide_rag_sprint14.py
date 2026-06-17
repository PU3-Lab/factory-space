from __future__ import annotations

import hashlib
import tempfile
from pathlib import Path

from sqlalchemy import create_engine, select
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.ext.compiler import compiles

from agents.operator_guide.rag_documents import ManualRagDocument
from agents.operator_guide.rag_ingestion import (
    FailedIngestionRow,
    ManualRagIngestionBatch,
    ManualRagIngestionRecord,
    ManualRagIngestionService,
    calculate_source_version,
)
from agents.operator_guide.rag_schema import (
    manual_rag_ingestion_failed_rows,
    manual_rag_ingestion_runs,
    metadata,
)
from agents.operator_guide.rag_store import SqlAlchemyManualRagStore
from agents.operator_guide.rag_upsert import ManualRagUpsertService


# SQLite 환경에서 JSONB 컴파일러가 에러를 내지 않도록 처리
@compiles(JSONB, "sqlite")
def compile_jsonb_sqlite(type_: JSONB, compiler: object, **kw: object) -> str:
    """SQLite 환경에서 JSONB 타입을 일반 JSON 컬럼 타입으로 컴파일합니다."""
    _ = type_, compiler, kw
    return "JSON"


# SQLite 환경에서 pgvector Vector 컴파일러가 에러를 내지 않도록 처리
try:
    from pgvector.sqlalchemy import Vector

    @compiles(Vector, "sqlite")
    def compile_vector_sqlite(type_: Vector, compiler: object, **kw: object) -> str:
        """SQLite 환경에서 pgvector Vector 타입을 TEXT 컬럼 타입으로 컴파일합니다."""
        _ = type_, compiler, kw
        return "TEXT"
except ImportError:
    pass


class FakeEmbeddingProvider:
    def __init__(self, should_fail_texts: list[str] | None = None) -> None:
        self.should_fail_texts = should_fail_texts or []
        self.calls: list[list[str]] = []

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        self.calls.append(texts)
        for t in texts:
            if t in self.should_fail_texts:
                raise ValueError(f"Failed to embed text: {t}")
        return [[0.1] * 1536 for _ in texts]


def test_source_version_generation() -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        (tmp_path / "equipment.csv").write_text("id,name\n1,furnace", encoding="utf-8")
        (tmp_path / "resources.csv").write_text("id,name\n2,iron", encoding="utf-8")

        ver = calculate_source_version(tmp_path)
        assert ver.startswith("csv-")
        assert ":git-" in ver


def test_partial_failure_retry_mechanism() -> None:
    doc1 = ManualRagDocument(
        doc_id="equipment:1",
        source_file="equipment.csv",
        source_row_id="1",
        title="제련기",
        content="furnace content",
        metadata={},
    )
    doc2 = ManualRagDocument(
        doc_id="resource:2",
        source_file="resources.csv",
        source_row_id="2",
        title="철광석",
        content="iron content",
        metadata={},
    )

    provider = FakeEmbeddingProvider(should_fail_texts=["iron content"])
    service = ManualRagIngestionService(provider)

    batch = service.build_batch(
        [doc1, doc2],
        existing_content_hashes={},
    )

    assert len(batch.records) == 1
    assert batch.records[0].doc_id == "equipment:1"

    assert len(batch.failed_rows) == 1
    assert batch.failed_rows[0].doc_id == "resource:2"
    assert "Failed to embed text: iron content" in batch.failed_rows[0].error_message

    assert len(provider.calls) == 3
    assert provider.calls[0] == ["furnace content", "iron content"]
    assert provider.calls[1] == ["furnace content"]
    assert provider.calls[2] == ["iron content"]


def test_ingestion_run_logging_in_db() -> None:
    engine = create_engine("sqlite://")
    metadata.create_all(engine)
    store = SqlAlchemyManualRagStore(engine)
    upsert_service = ManualRagUpsertService(store)

    run_id = "test-run-123"
    source_version = "csv-test-version"

    batch = ManualRagIngestionBatch(
        records=[
            ManualRagIngestionRecord(
                doc_id="equipment:1",
                source_file="equipment.csv",
                source_row_id="1",
                title="제련기",
                embedding_text="furnace content",
                content_hash="hash1",
                metadata={},
                embedding=[0.1] * 1536,
            )
        ],
        content_hashes={
            "equipment:1": "hash1",
            "resource:2": "hash2",
        },
        failed_rows=[
            FailedIngestionRow(
                doc_id="resource:2",
                source_file="resources.csv",
                source_row_id="2",
                title="철광석",
                error_message="Embedding API error",
            )
        ],
    )

    upsert_service.upsert_batch(
        batch,
        run_id=run_id,
        source_version=source_version,
        dry_run=False,
    )

    with engine.begin() as conn:
        run_row = conn.execute(
            select(manual_rag_ingestion_runs).where(
                manual_rag_ingestion_runs.c.run_id == run_id
            )
        ).first()
        assert run_row is not None
        assert run_row.status == "success"
        assert run_row.inserted == 1
        assert run_row.failed == 1
        assert run_row.source_version == source_version

        failed_row = conn.execute(
            select(manual_rag_ingestion_failed_rows).where(
                manual_rag_ingestion_failed_rows.c.run_id == run_id
            )
        ).first()
        assert failed_row is not None
        assert failed_row.source_file == "resources.csv"
        assert failed_row.source_row_id == "2"
        assert failed_row.error_message == "Embedding API error"


def test_auto_retry_on_next_run() -> None:
    doc1 = ManualRagDocument(
        doc_id="equipment:1",
        source_file="equipment.csv",
        source_row_id="1",
        title="제련기",
        content="furnace content",
        metadata={},
    )
    doc2 = ManualRagDocument(
        doc_id="resource:2",
        source_file="resources.csv",
        source_row_id="2",
        title="철광석",
        content="iron content",
        metadata={},
    )

    doc1_hash = hashlib.sha256(b"furnace content").hexdigest()
    existing_hashes = {
        "equipment:1": doc1_hash,
    }

    provider = FakeEmbeddingProvider()
    service = ManualRagIngestionService(provider)

    batch = service.build_batch(
        [doc1, doc2],
        existing_content_hashes=existing_hashes,
    )

    assert len(batch.records) == 1
    assert batch.records[0].doc_id == "resource:2"
    assert provider.calls == [["iron content"]]
