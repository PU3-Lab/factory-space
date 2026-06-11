"""PostgreSQL storage for Manual Q&A RAG documents."""

from __future__ import annotations

from sqlalchemy import Engine, Select, create_engine, func, select, update
from sqlalchemy.dialects.postgresql import insert

from agents.operator_guide.rag_ingestion import ManualRagIngestionRecord
from agents.operator_guide.rag_schema import manual_rag_documents


class SqlAlchemyManualRagStore:
    """Store Manual Q&A RAG records in PostgreSQL + pgvector."""

    def __init__(self, engine: Engine) -> None:
        self._engine = engine

    def list_active_content_hashes(self) -> dict[str, str]:
        statement: Select[tuple[str, str]] = select(
            manual_rag_documents.c.doc_id,
            manual_rag_documents.c.content_hash,
        ).where(manual_rag_documents.c.is_active.is_(True))
        with self._engine.begin() as connection:
            return dict(connection.execute(statement).all())

    def upsert_records(self, records: list[ManualRagIngestionRecord]) -> None:
        if not records:
            return
        values = [_record_to_values(record) for record in records]
        statement = insert(manual_rag_documents).values(values)
        update_values = {
            "source_file": statement.excluded.source_file,
            "source_row_id": statement.excluded.source_row_id,
            "title": statement.excluded.title,
            "content": statement.excluded.content,
            "content_hash": statement.excluded.content_hash,
            "metadata_json": statement.excluded.metadata_json,
            "embedding": statement.excluded.embedding,
            "is_active": True,
            "updated_at": func.now(),
        }
        statement = statement.on_conflict_do_update(
            index_elements=[manual_rag_documents.c.doc_id],
            set_=update_values,
        )
        with self._engine.begin() as connection:
            connection.execute(statement)

    def deactivate_missing(self, doc_ids: list[str]) -> None:
        if not doc_ids:
            return
        statement = (
            update(manual_rag_documents)
            .where(manual_rag_documents.c.doc_id.in_(doc_ids))
            .values(is_active=False, updated_at=func.now())
        )
        with self._engine.begin() as connection:
            connection.execute(statement)


def create_manual_rag_store(database_url: str) -> SqlAlchemyManualRagStore:
    """Create a PostgreSQL-backed Manual Q&A RAG document store."""

    return SqlAlchemyManualRagStore(create_engine(database_url))


def _record_to_values(record: ManualRagIngestionRecord) -> dict[str, object]:
    return {
        "doc_id": record.doc_id,
        "source_file": record.source_file,
        "source_row_id": record.source_row_id,
        "title": record.title,
        "content": record.embedding_text,
        "content_hash": record.content_hash,
        "metadata_json": record.metadata,
        "embedding": record.embedding,
        "is_active": True,
    }
