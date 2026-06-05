"""Run the production FastAPI backend server."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import uvicorn

# Allow importing from scripts directory
backend_root = Path(__file__).resolve().parents[1]
if str(backend_root) not in sys.path:
    sys.path.insert(0, str(backend_root))
from scripts.run_server import (  # noqa: E402
    ensure_ollama_running,
    load_env_file,
)


def prepare_environment(backend_root: Path) -> None:
    """Prepare cwd, import path, and default .env.prod settings for the server."""

    os.chdir(backend_root)
    src_path = str(backend_root / "src")
    if src_path not in sys.path:
        sys.path.insert(0, src_path)
    load_env_file(backend_root / ".env.prod")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the Factory Space backend in PRODUCTION mode.")
    parser.add_argument("--host", default=os.getenv("HOST", "0.0.0.0"))
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.getenv("PORT", "18000")),
        help="Server port. Defaults to 18000.",
    )
    parser.add_argument("--app", default=os.getenv("APP", "app:create_app"))
    return parser.parse_args()


def main() -> None:
    backend_root = Path(__file__).resolve().parents[1]
    prepare_environment(backend_root)

    args = parse_args()

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

    print(f"[Prod Server] Starting production server on {args.host}:{args.port} (reload=False)")
    uvicorn.run(
        args.app,
        host=args.host,
        port=args.port,
        reload=False,
        factory=True,
    )


if __name__ == "__main__":
    main()
