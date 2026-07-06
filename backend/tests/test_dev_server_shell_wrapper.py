from __future__ import annotations

from pathlib import Path


def test_root_dev_server_wrapper_prefers_uv_run() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    script = (repo_root / "scripts" / "run_dev_server.sh").read_text(encoding="utf-8")

    assert "command -v uv" in script
    assert "uv run python scripts/run_dev_server.py" in script


def test_backend_dev_server_wrapper_prefers_uv_run() -> None:
    backend_root = Path(__file__).resolve().parents[1]
    script = (backend_root / "run_dev_server.sh").read_text(encoding="utf-8")

    assert "command -v uv" in script
    assert "uv run python scripts/run_dev_server.py" in script
