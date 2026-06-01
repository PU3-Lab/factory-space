"""Local development entrypoint."""

from __future__ import annotations

import uvicorn


def main() -> None:
    """Run the local development server."""

    uvicorn.run(
        "app:create_app",
        host="127.0.0.1",
        port=18000,
        reload=True,
        factory=True,
    )


if __name__ == "__main__":
    main()
