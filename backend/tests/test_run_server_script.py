from __future__ import annotations

import os
from pathlib import Path

import pytest

from scripts import run_dev_server, run_server


def test_load_env_file_sets_values_without_overwriting_existing_env(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    env_file = tmp_path / ".env"
    env_file.write_text(
        "\n".join(
            [
                "HOST=0.0.0.0",
                "PORT=19000",
                "APP='custom:create_app'",
                "# ignored comment",
                "FACTORY_LLM_DEFAULT_PROVIDER=local",
                "",
            ]
        ),
        encoding="utf-8",
    )
    monkeypatch.setenv("HOST", "127.0.0.1")
    monkeypatch.delenv("PORT", raising=False)
    monkeypatch.delenv("APP", raising=False)
    monkeypatch.delenv("FACTORY_LLM_DEFAULT_PROVIDER", raising=False)

    run_server.load_env_file(env_file)

    assert os.environ["HOST"] == "127.0.0.1"
    assert os.environ["PORT"] == "19000"
    assert os.environ["APP"] == "custom:create_app"
    assert os.environ["FACTORY_LLM_DEFAULT_PROVIDER"] == "local"


def test_prepare_environment_loads_backend_env_before_parsing_args(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    env_file = tmp_path / ".env"
    env_file.write_text("PORT=19001\n", encoding="utf-8")
    monkeypatch.delenv("PORT", raising=False)
    monkeypatch.setattr(run_server.sys, "argv", ["run_server.py"])

    run_server.prepare_environment(tmp_path)

    args = run_server.parse_args()

    assert args.port == 19001


def test_prepare_dev_environment_loads_env_dev_before_parsing_args(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    env_file = tmp_path / ".env"
    env_file.write_text("PORT=19001\n", encoding="utf-8")
    dev_env_file = tmp_path / ".env.dev"
    dev_env_file.write_text(
        "\n".join(
            [
                "PORT=19002",
                "FACTORY_LLM_DEFAULT_MODEL=from-dev-env",
            ]
        ),
        encoding="utf-8",
    )
    monkeypatch.delenv("PORT", raising=False)
    monkeypatch.delenv("FACTORY_ENV_FILE", raising=False)
    monkeypatch.delenv("FACTORY_LLM_DEFAULT_MODEL", raising=False)
    monkeypatch.delenv("DATABASE_URL", raising=False)
    monkeypatch.setattr(run_dev_server.sys, "argv", ["run_dev_server.py"])

    run_dev_server.prepare_environment(tmp_path)

    args = run_dev_server.parse_args()

    assert args.port == 19002
    assert os.environ["FACTORY_ENV_FILE"] == ".env.dev"
    assert os.environ["FACTORY_LLM_DEFAULT_MODEL"] == "from-dev-env"
    assert os.environ["DATABASE_URL"] == run_dev_server.DEV_DATABASE_URL
    assert os.environ["FACTORY_AUTO_INGEST_RECIPES"] == "true"
