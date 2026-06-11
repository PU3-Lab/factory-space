"""Tests for the Manual Q&A RAG PostgreSQL + pgvector schema."""

from __future__ import annotations

from pathlib import Path

from agents.operator_guide.rag_schema import manual_rag_documents, metadata

ROOT = Path(__file__).resolve().parents[1]


def test_manual_rag_documents_schema_has_expected_columns() -> None:
    columns = manual_rag_documents.c

    assert manual_rag_documents.name == "manual_rag_documents"
    assert "manual_rag_documents" in metadata.tables
    assert columns.id.primary_key
    assert columns.doc_id.unique
    assert columns.source_file.nullable is False
    assert columns.source_row_id.nullable is False
    assert columns.title.nullable is False
    assert columns.content.nullable is False
    assert columns.content_hash.nullable is False
    assert columns.metadata_json.nullable is False
    assert columns.embedding.nullable is False
    assert columns.is_active.default.arg is True
    assert columns.created_at.nullable is False
    assert columns.updated_at.nullable is False


def test_manual_rag_documents_schema_has_expected_indexes() -> None:
    indexes = {index.name: index for index in manual_rag_documents.indexes}

    assert "ix_manual_rag_documents_content_hash" in indexes
    assert "ix_manual_rag_documents_is_active" in indexes
    assert "ix_manual_rag_documents_metadata_json" in indexes
    assert "ix_manual_rag_documents_embedding" in indexes
    assert (
        indexes["ix_manual_rag_documents_embedding"].dialect_options["postgresql"][
            "using"
        ]
        == "ivfflat"
    )


def test_alembic_config_points_to_backend_migrations() -> None:
    alembic_ini = ROOT / "alembic.ini"

    assert alembic_ini.exists()
    assert "script_location = migrations" in alembic_ini.read_text(encoding="utf-8")


def test_initial_migration_creates_pgvector_document_table() -> None:
    migration = ROOT / "migrations" / "versions" / "0001_create_manual_rag_documents.py"
    migration_text = migration.read_text(encoding="utf-8")

    assert "CREATE EXTENSION IF NOT EXISTS vector" in migration_text
    assert "manual_rag_documents" in migration_text
    assert "Vector(1536)" in migration_text
    assert "ix_manual_rag_documents_embedding" in migration_text
