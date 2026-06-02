"""Run the local FastAPI backend server."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import uvicorn


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
    os.chdir(backend_root)
    sys.path.insert(0, str(backend_root / "src"))

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
