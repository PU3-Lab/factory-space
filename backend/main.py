"""Local development entrypoint."""

from __future__ import annotations

import os
import sys
from pathlib import Path

import uvicorn

# Allow importing from scripts directory
backend_root = Path(__file__).resolve().parent
if str(backend_root) not in sys.path:
    sys.path.insert(0, str(backend_root))
from scripts.run_server import ensure_ollama_running, prepare_environment  # noqa: E402


def main() -> None:
    """Run the local development server."""
    prepare_environment(backend_root)

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

    uvicorn.run(
        "app:create_app",
        host="127.0.0.1",
        port=18000,
        reload=True,
        factory=True,
    )


if __name__ == "__main__":
    main()
