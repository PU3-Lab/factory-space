"""create manual RAG documents table

Revision ID: 0001_create_manual_rag_documents
Revises:
Create Date: 2026-06-09
"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op
from pgvector.sqlalchemy import Vector
from sqlalchemy.dialects import postgresql

revision: str = "0001_create_manual_rag_documents"
down_revision: str | None = None
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.execute("CREATE EXTENSION IF NOT EXISTS vector")
    op.create_table(
        "manual_rag_documents",
        sa.Column("id", sa.Integer(), primary_key=True),
        sa.Column("doc_id", sa.String(length=255), nullable=False, unique=True),
        sa.Column("source_file", sa.String(length=255), nullable=False),
        sa.Column("source_row_id", sa.String(length=255), nullable=False),
        sa.Column("title", sa.String(length=255), nullable=False),
        sa.Column("content", sa.Text(), nullable=False),
        sa.Column("content_hash", sa.String(length=64), nullable=False),
        sa.Column("metadata_json", postgresql.JSONB(), nullable=False),
        sa.Column("embedding", Vector(1536), nullable=False),
        sa.Column("is_active", sa.Boolean(), server_default="true", nullable=False),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
        sa.Column(
            "updated_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
    )
    op.create_index(
        "ix_manual_rag_documents_content_hash",
        "manual_rag_documents",
        ["content_hash"],
    )
    op.create_index(
        "ix_manual_rag_documents_is_active",
        "manual_rag_documents",
        ["is_active"],
    )
    op.create_index(
        "ix_manual_rag_documents_metadata_json",
        "manual_rag_documents",
        ["metadata_json"],
        postgresql_using="gin",
    )
    op.create_index(
        "ix_manual_rag_documents_embedding",
        "manual_rag_documents",
        ["embedding"],
        postgresql_using="ivfflat",
        postgresql_ops={"embedding": "vector_cosine_ops"},
    )


def downgrade() -> None:
    op.drop_index(
        "ix_manual_rag_documents_embedding",
        table_name="manual_rag_documents",
    )
    op.drop_index(
        "ix_manual_rag_documents_metadata_json",
        table_name="manual_rag_documents",
    )
    op.drop_index(
        "ix_manual_rag_documents_is_active",
        table_name="manual_rag_documents",
    )
    op.drop_index(
        "ix_manual_rag_documents_content_hash",
        table_name="manual_rag_documents",
    )
    op.drop_table("manual_rag_documents")
