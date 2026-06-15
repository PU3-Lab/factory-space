"""operator_guide RAG 문서를 저장할 PostgreSQL + pgvector 테이블 정의.

초보자용 설명:
    이 파일은 SQLAlchemy로 `manual_rag_documents` 테이블 모양을 정의한다.
    실제 migration과 테스트가 이 metadata를 기준으로 테이블/인덱스를 확인한다.
"""

from __future__ import annotations

from pgvector.sqlalchemy import Vector
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
    Column("created_at", DateTime(timezone=True), nullable=False, server_default=func.now()),
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
