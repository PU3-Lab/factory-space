"""Run the local FastAPI backend server."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import uvicorn


def load_env_file(env_file: Path) -> None:
    """Load simple KEY=VALUE entries from an env file without overriding env."""

    if not env_file.exists():
        return

    for raw_line in env_file.read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key:
            continue
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        os.environ.setdefault(key, value)


def prepare_environment(backend_root: Path) -> None:
    """Prepare cwd, import path, and default .env settings for the server."""

    os.chdir(backend_root)
    src_path = str(backend_root / "src")
    if src_path not in sys.path:
        sys.path.insert(0, src_path)
    load_env_file(backend_root / ".env")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the Factory Space backend.")
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
    return parser.parse_args()


def main() -> None:
    backend_root = Path(__file__).resolve().parents[1]
    prepare_environment(backend_root)

    args = parse_args()
    uvicorn.run(
        args.app,
        host=args.host,
        port=args.port,
        reload=not args.no_reload,
        factory=True,
    )


if __name__ == "__main__":
    main()
