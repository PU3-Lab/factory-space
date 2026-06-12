"""ASGI application for the Factory Space backend."""

from __future__ import annotations

from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

from fastapi import FastAPI

from agent_connection.router import router as agent_connection_router
from agents.material_generation.router import router as material_generation_router
from agents.pipeline import AgentPipeline
from docs_router import router as docs_router
from websocket_gateway.gateway import router as websocket_router


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    """Initialize application-scoped runtime dependencies."""

    app.state.agent_pipeline = AgentPipeline()
    yield
    del app.state.agent_pipeline


def create_app() -> FastAPI:
    """Create the FastAPI application."""

    app = FastAPI(title="Factory Space Backend", lifespan=lifespan)

    @app.get("/health")
    async def health() -> dict[str, str]:
        return {"status": "ok"}

    app.include_router(agent_connection_router)
    app.include_router(material_generation_router, prefix="/api/v1")
    app.include_router(docs_router)
    app.include_router(websocket_router)
    return app
