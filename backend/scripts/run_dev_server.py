"""Run the development FastAPI backend server.

Development defaults:
- Loads ``.env.dev`` (does not override already-set env vars).
- Uses a dedicated test SQLite DB (``factory_space.test.db``) so the live DB
  is never touched during development.
- Applies Alembic migrations (``upgrade head``) on startup so a fresh checkout
  never hits ``no such table`` errors.
- Enables uvicorn auto-reload on ``127.0.0.1``.
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import uvicorn
from alembic import command
from alembic.config import Config

# Allow importing helpers from the scripts directory.
backend_root = Path(__file__).resolve().parents[1]
if str(backend_root) not in sys.path:
    sys.path.insert(0, str(backend_root))
from scripts.run_server import (  # noqa: E402
    ensure_ollama_running,
    load_env_file,
)

DEV_DATABASE_URL = "sqlite:///./factory_space.test.db"


def prepare_environment(backend_root: Path) -> None:
    """Prepare cwd, import path, and default .env.dev settings for the server."""

    os.chdir(backend_root)
    src_path = str(backend_root / "src")
    if src_path not in sys.path:
        sys.path.insert(0, src_path)
    configured_env_file = Path(os.environ.setdefault("FACTORY_ENV_FILE", ".env.dev"))
    env_file = (
        configured_env_file
        if configured_env_file.is_absolute()
        else backend_root / configured_env_file
    )
    load_env_file(env_file)
    # Fall back to the dedicated test DB unless an env file/shell already set one.
    os.environ.setdefault("DATABASE_URL", DEV_DATABASE_URL)
    os.environ.setdefault("FACTORY_AUTO_INGEST_RECIPES", "true")


def run_migrations(backend_root: Path) -> None:
    """Apply Alembic migrations up to head against the configured database."""

    config = Config(str(backend_root / "alembic.ini"))
    db_url = os.environ.get("FACTORY_DATABASE_URL") or os.environ.get("DATABASE_URL")
    print(f"[Dev Server] Applying migrations (alembic upgrade head) on {db_url}")
    command.upgrade(config, "head")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the Factory Space backend in DEVELOPMENT mode."
    )
    parser.add_argument("--host", default=os.getenv("HOST", "127.0.0.1"))
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.getenv("PORT", "18000")),
        help="Server port. Defaults to 18000 because 8000 can be blocked on Windows.",
    )
    parser.add_argument("--app", default=os.getenv("APP", "app:create_app"))
    parser.add_argument(
        "--no-reload",
        action="store_true",
        help="Disable uvicorn reload.",
    )
    parser.add_argument(
        "--no-migrate",
        action="store_true",
        help="Skip the automatic 'alembic upgrade head' on startup.",
    )
    return parser.parse_args()


def main() -> None:
    backend_root = Path(__file__).resolve().parents[1]
    prepare_environment(backend_root)

    args = parse_args()

    if not args.no_migrate:
        run_migrations(backend_root)

    # If local provider is used as a fallback, ensure Ollama is running.
    providers = [
        os.getenv("FACTORY_LLM_DEFAULT_PROVIDER"),
        os.getenv("FACTORY_LLM_FALLBACK1_PROVIDER"),
        os.getenv("FACTORY_LLM_FALLBACK2_PROVIDER"),
    ]
    if "local" in providers:
        base_urls = [
            os.getenv("FACTORY_LLM_DEFAULT_BASE_URL"),
            os.getenv("FACTORY_LLM_FALLBACK1_BASE_URL"),
            os.getenv("FACTORY_LLM_FALLBACK2_BASE_URL"),
        ]
        for provider, base_url in zip(providers, base_urls):
            if provider == "local":
                target_url = base_url or "http://localhost:11434/v1"
                ensure_ollama_running(target_url)
                break

    print(
        f"[Dev Server] Starting development server on {args.host}:{args.port} "
        f"(reload={not args.no_reload})"
    )
    uvicorn.run(
        args.app,
        host=args.host,
        port=args.port,
        reload=not args.no_reload,
        factory=True,
    )


if __name__ == "__main__":
    main()
