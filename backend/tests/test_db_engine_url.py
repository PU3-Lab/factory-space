"""Tests for separating the app database URL from the operator_guide RAG DB."""

from __future__ import annotations

import pytest

from db.engine import get_database_url


def test_app_database_url_uses_database_url(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("DATABASE_URL", "sqlite:///./factory_space.local.db")
    monkeypatch.setenv(
        "FACTORY_RAG_DATABASE_URL",
        "postgresql+psycopg://factory_space:factory_space@127.0.0.1:5433/factory_space",
    )

    assert get_database_url() == "sqlite:///./factory_space.local.db"


def test_app_database_url_does_not_fallback_to_rag_database(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("DATABASE_URL", raising=False)
    monkeypatch.setenv(
        "FACTORY_RAG_DATABASE_URL",
        "postgresql+psycopg://factory_space:factory_space@127.0.0.1:5433/factory_space",
    )
    monkeypatch.setenv(
        "FACTORY_DATABASE_URL",
        "postgresql+psycopg://legacy:legacy@127.0.0.1:5433/factory_space",
    )

    assert get_database_url() == "sqlite:///./factory_space.db"
