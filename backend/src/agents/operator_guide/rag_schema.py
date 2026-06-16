"""operator_guide RAG 문서를 저장할 PostgreSQL + pgvector 테이블 정의.

초보자용 설명:
    이 파일은 SQLAlchemy로 `manual_rag_documents` 테이블 모양을 정의한다.
    실제 migration과 테스트가 이 metadata를 기준으로 테이블/인덱스를 확인한다.
"""

from __future__ import annotations

try:
    from pgvector.sqlalchemy import Vector
except ImportError:
    from typing import Any

    from sqlalchemy.types import UserDefinedType

    class Vector(UserDefinedType):  # type: ignore
        """Dummy Vector type for environments without pgvector (e.g. SQLite local development)."""

        def __init__(self, dimensions: int | None = None) -> None:
            self.dimensions = dimensions

        def get_col_spec(self, **kw: Any) -> str:  # noqa: ANN401
            return f"VECTOR({self.dimensions})"


from sqlalchemy import (
    Boolean,
    Column,
    DateTime,
    Index,
    Integer,
    MetaData,
    String,
    Table,
    Text,
    func,
)
from sqlalchemy.dialects.postgresql import JSONB

EMBEDDING_DIMENSIONS = 1536
"""OpenAI `text-embedding-3-small` 기준 embedding vector 길이."""

metadata = MetaData()

manual_rag_documents = Table(
    # CSV에서 만든 RAG 문서와 embedding vector를 함께 저장하는 테이블이다.
    "manual_rag_documents",
    metadata,
    Column("id", Integer, primary_key=True),
    Column("doc_id", String(255), nullable=False, unique=True),
    Column("source_file", String(255), nullable=False),
    Column("source_row_id", String(255), nullable=False),
    Column("title", String(255), nullable=False),
    Column("content", Text, nullable=False),
    Column("content_hash", String(64), nullable=False),
    Column("metadata_json", JSONB, nullable=False),
    Column("embedding", Vector(EMBEDDING_DIMENSIONS), nullable=False),
    Column("is_active", Boolean, nullable=False, default=True, server_default="true"),
    Column(
        "created_at", DateTime(timezone=True), nullable=False, server_default=func.now()
    ),
    Column(
        "updated_at",
        DateTime(timezone=True),
        nullable=False,
        server_default=func.now(),
        onupdate=func.now(),
    ),
    Index("ix_manual_rag_documents_content_hash", "content_hash"),
    Index("ix_manual_rag_documents_is_active", "is_active"),
    Index(
        "ix_manual_rag_documents_metadata_json",
        "metadata_json",
        postgresql_using="gin",
    ),
    Index(
        "ix_manual_rag_documents_embedding",
        "embedding",
        postgresql_using="ivfflat",
        postgresql_ops={"embedding": "vector_cosine_ops"},
    ),
)


# Ingestion이 실행될 때마다 전체 상태와 통계를 저장하는 실행 이력 테이블입니다.
manual_rag_ingestion_runs = Table(
    "manual_rag_ingestion_runs",
    metadata,
    Column("id", Integer, primary_key=True),
    Column("run_id", String(255), nullable=False, unique=True),
    Column("status", String(50), nullable=False),  # 'started', 'success', 'failed'
    Column("inserted", Integer, nullable=True),
    Column("updated", Integer, nullable=True),
    Column("skipped", Integer, nullable=True),
    Column("deactivated", Integer, nullable=True),
    Column("failed", Integer, nullable=True),
    Column("source_version", String(255), nullable=True),
    Column("error_message", Text, nullable=True),
    Column("started_at", DateTime(timezone=True), nullable=False, server_default=func.now()),
    Column("completed_at", DateTime(timezone=True), nullable=True),
    Index("ix_manual_rag_ingestion_runs_run_id", "run_id"),
)

# Ingestion 중 임베딩이나 변환에 실패한 개별 Row의 상세 에러 원인을 기록하는 테이블입니다.
manual_rag_ingestion_failed_rows = Table(
    "manual_rag_ingestion_failed_rows",
    metadata,
    Column("id", Integer, primary_key=True),
    Column("run_id", String(255), nullable=False),
    Column("source_file", String(255), nullable=False),
    Column("source_row_id", String(255), nullable=False),
    Column("title", String(255), nullable=False),
    Column("error_message", Text, nullable=False),
    Column("failed_at", DateTime(timezone=True), nullable=False, server_default=func.now()),
    Index("ix_manual_rag_ingestion_failed_rows_run_id", "run_id"),
)
