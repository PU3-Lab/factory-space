"""ASGI application for the Factory Space backend."""

from __future__ import annotations

from fastapi import FastAPI

from websocket_gateway.gateway import router as websocket_router


def create_app() -> FastAPI:
    """Create the FastAPI application."""

    app = FastAPI(title="Factory Space Backend")

    @app.get("/health")
    async def health() -> dict[str, str]:
        return {"status": "ok"}

    app.include_router(websocket_router)
    return app


app = create_app()
