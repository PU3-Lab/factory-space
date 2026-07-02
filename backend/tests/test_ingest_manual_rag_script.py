"""Tests for the Manual Q&A RAG ingestion CLI output."""

from __future__ import annotations

import pytest

from agents.operator_guide.rag_upsert import ManualRagUpsertSummary
from scripts.ingest_manual_rag import (
    _database_url,
    format_database_error,
    format_summary,
)


def test_dry_run_summary_shows_counts_and_apply_command() -> None:
    summary = ManualRagUpsertSummary(
        inserted=2,
        updated=1,
        skipped=142,
        deactivated=0,
        failed=0,
    )

    output = format_summary(summary, dry_run=True)

    assert "Dry-run complete." in output
    assert "inserted=2" in output
    assert "updated=1" in output
    assert "skipped=142" in output
    assert "deactivated=0" in output
    assert "uv run --env-file .env.prod python scripts/ingest_manual_rag.py" in output


def test_database_error_message_shows_setup_commands() -> None:
    output = format_database_error()

    assert "RAG database connection failed." in output
    assert "docker compose -f docker-compose.rag.yml up -d" in output
    assert "uv run --env-file .env.prod alembic upgrade head" in output


def test_database_url_prefers_rag_specific_env(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("FACTORY_RAG_DATABASE_URL", "postgresql+psycopg://rag-db")
    monkeypatch.setenv("FACTORY_DATABASE_URL", "postgresql+psycopg://legacy-db")
    monkeypatch.setenv("DATABASE_URL", "sqlite:///./factory_space.db")

    assert _database_url() == "postgresql+psycopg://rag-db"


def test_database_url_does_not_use_application_database_env(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("FACTORY_RAG_DATABASE_URL", raising=False)
    monkeypatch.delenv("FACTORY_DATABASE_URL", raising=False)
    monkeypatch.setenv("DATABASE_URL", "sqlite:///./factory_space.db")

    assert _database_url() is None
