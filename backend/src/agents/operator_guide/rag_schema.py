"""PostgreSQL schema metadata for operator_guide RAG documents."""

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

metadata = MetaData()

manual_rag_documents = Table(
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
