"""Manual Q&A RAG 문서를 PostgreSQL + pgvector에 저장하는 모듈.

초보자용 설명:
    `rag_ingestion.py`에서 만든 record를 실제 DB 테이블에 넣거나 갱신한다.
    content hash를 이용해 바뀐 문서만 update하고, 사라진 문서는 비활성화한다.
"""

from __future__ import annotations

from sqlalchemy import Engine, Select, create_engine, func, select, update
from sqlalchemy.dialects.postgresql import insert

from agents.operator_guide.rag_ingestion import ManualRagIngestionRecord
from agents.operator_guide.rag_schema import manual_rag_documents


class SqlAlchemyManualRagStore:
    """SQLAlchemy를 사용해 RAG 문서를 DB에 저장하는 저장소 클래스."""

    def __init__(self, engine: Engine) -> None:
        self._engine = engine

    def list_active_content_hashes(self) -> dict[str, str]:
        """현재 DB에 살아 있는 문서의 content hash를 doc_id 기준으로 가져온다."""

        statement: Select[tuple[str, str]] = select(
            manual_rag_documents.c.doc_id,
            manual_rag_documents.c.content_hash,
        ).where(manual_rag_documents.c.is_active.is_(True))
        with self._engine.begin() as connection:
            return dict(connection.execute(statement).all())

    def upsert_records(self, records: list[ManualRagIngestionRecord]) -> None:
        """새 문서는 insert하고, 이미 있는 문서는 update한다."""

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
        """CSV에서 사라진 문서를 삭제하지 않고 inactive 상태로 바꾼다."""

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
    """DB URL로 PostgreSQL 기반 RAG 저장소를 만든다."""

    return SqlAlchemyManualRagStore(create_engine(database_url))


def _record_to_values(record: ManualRagIngestionRecord) -> dict[str, object]:
    """ingestion record를 SQLAlchemy insert 값으로 바꾼다."""

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
