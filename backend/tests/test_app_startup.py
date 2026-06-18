from __future__ import annotations

import os
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

import app as app_module


def test_app_lifespan_loads_backend_env_before_creating_pipeline(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    called = False

    def record_load_backend_env() -> None:
        nonlocal called
        called = True

    monkeypatch.setattr(app_module, "_load_backend_env", record_load_backend_env)
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK1_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK2_PROVIDER", "none")

    with TestClient(app_module.create_app()) as client:
        response = client.get("/health")

    assert response.status_code == 200
    assert called is True


def test_load_env_file_sets_missing_values_without_overwriting_existing_env(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    env_file = tmp_path / ".env"
    env_file.write_text(
        "\n".join(
            [
                "FACTORY_LLM_DEFAULT_PROVIDER=local",
                "FACTORY_LLM_DEFAULT_MODEL='gemma4:e2b-mlx'",
                "# ignored comment",
                "",
            ]
        ),
        encoding="utf-8",
    )
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")
    monkeypatch.delenv("FACTORY_LLM_DEFAULT_MODEL", raising=False)

    app_module._load_env_file(env_file)

    assert os.environ["FACTORY_LLM_DEFAULT_PROVIDER"] == "none"
    assert os.environ["FACTORY_LLM_DEFAULT_MODEL"] == "gemma4:e2b-mlx"


def test_load_backend_env_uses_configured_env_file(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    env_file = tmp_path / ".env.dev"
    env_file.write_text("FACTORY_CUSTOM_ENV_MARKER=from-dev\n", encoding="utf-8")
    monkeypatch.setenv("FACTORY_ENV_FILE", str(env_file))
    monkeypatch.delenv("FACTORY_CUSTOM_ENV_MARKER", raising=False)

    app_module._load_backend_env()

    assert os.environ["FACTORY_CUSTOM_ENV_MARKER"] == "from-dev"


def test_maybe_ingest_dev_recipes_ingests_when_enabled_and_empty(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    called = False

    def count_no_recipes() -> int:
        return 0

    def record_ingest() -> None:
        nonlocal called
        called = True

    monkeypatch.setenv("FACTORY_AUTO_INGEST_RECIPES", "true")
    monkeypatch.setattr(app_module, "_count_recipes", count_no_recipes)
    monkeypatch.setattr(app_module, "_ingest_recipes_from_csv", record_ingest)

    app_module._maybe_ingest_dev_recipes()

    assert called is True


def test_maybe_ingest_dev_recipes_skips_when_recipes_exist(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    called = False

    def count_existing_recipes() -> int:
        return 12

    def record_ingest() -> None:
        nonlocal called
        called = True

    monkeypatch.setenv("FACTORY_AUTO_INGEST_RECIPES", "true")
    monkeypatch.setattr(app_module, "_count_recipes", count_existing_recipes)
    monkeypatch.setattr(app_module, "_ingest_recipes_from_csv", record_ingest)

    app_module._maybe_ingest_dev_recipes()

    assert called is False
