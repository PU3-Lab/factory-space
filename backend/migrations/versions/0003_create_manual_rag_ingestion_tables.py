"""create manual RAG ingestion tracking tables

Revision ID: 0003_manual_rag_ingest
Revises: 0002_create_material_tables
Create Date: 2026-06-16
"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "0003_manual_rag_ingest"
down_revision: str | None = "0002_create_material_tables"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "manual_rag_ingestion_runs",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("run_id", sa.String(length=255), nullable=False, unique=True),
        sa.Column("status", sa.String(length=50), nullable=False),
        sa.Column("inserted", sa.Integer(), nullable=True),
        sa.Column("updated", sa.Integer(), nullable=True),
        sa.Column("skipped", sa.Integer(), nullable=True),
        sa.Column("deactivated", sa.Integer(), nullable=True),
        sa.Column("failed", sa.Integer(), nullable=True),
        sa.Column("source_version", sa.String(length=255), nullable=True),
        sa.Column("error_message", sa.Text(), nullable=True),
        sa.Column(
            "started_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
        sa.Column("completed_at", sa.DateTime(timezone=True), nullable=True),
    )
    op.create_index(
        "ix_manual_rag_ingestion_runs_run_id",
        "manual_rag_ingestion_runs",
        ["run_id"],
    )

    op.create_table(
        "manual_rag_ingestion_failed_rows",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("run_id", sa.String(length=255), nullable=False),
        sa.Column("source_file", sa.String(length=255), nullable=False),
        sa.Column("source_row_id", sa.String(length=255), nullable=False),
        sa.Column("title", sa.String(length=255), nullable=False),
        sa.Column("error_message", sa.Text(), nullable=False),
        sa.Column(
            "failed_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
    )
    op.create_index(
        "ix_manual_rag_ingestion_failed_rows_run_id",
        "manual_rag_ingestion_failed_rows",
        ["run_id"],
    )


def downgrade() -> None:
    op.drop_index(
        "ix_manual_rag_ingestion_failed_rows_run_id",
        table_name="manual_rag_ingestion_failed_rows",
    )
    op.drop_table("manual_rag_ingestion_failed_rows")
    op.drop_index(
        "ix_manual_rag_ingestion_runs_run_id",
        table_name="manual_rag_ingestion_runs",
    )
    op.drop_table("manual_rag_ingestion_runs")
