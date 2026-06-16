"""ASGI application for the Factory Space backend."""

from __future__ import annotations

import logging
import os
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

from fastapi import FastAPI

from agent_connection.router import router as agent_connection_router
from agents.material_generation.events import MaterialEventPublisher
from agents.material_generation.router import router as material_generation_router
from agents.operator_guide.debug_router import router as debug_router
from agents.operator_guide.multi_question_rag_retriever import MultiQuestionRagRetriever
from agents.operator_guide.rag_embedding import create_embedding_provider
from agents.operator_guide.rag_retriever import ManualRagRetriever
from agents.operator_guide.rag_store import SqlAlchemyManualRagStore
from agents.operator_guide.service import ManualQAService
from agents.pipeline import AgentPipeline
from db.engine import engine
from docs_router import router as docs_router
from manual_qa_docs.router import router as manual_qa_docs_router
from websocket_gateway.gateway import router as websocket_router


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    """Initialize application-scoped runtime dependencies."""
    # Ensure the background executor is initialized and active for this app context
    MaterialEventPublisher.reset_executor(wait=False)

    # Initialize RAG runtime for operator_guide
    if os.environ.get("FACTORY_RAG_RUNTIME_MOCK") != "true":
        try:
            embedding_provider = create_embedding_provider()
            search_store = SqlAlchemyManualRagStore(engine)
            retriever = ManualRagRetriever(
                embedding_provider=embedding_provider,
                search_store=search_store,
            )
            rag_runtime = MultiQuestionRagRetriever(rag_retriever=retriever)
            ManualQAService.set_global_rag_runtime(rag_runtime)
        except Exception as exc:
            logging.getLogger("app").warning(
                "Failed to initialize RAG runtime (falling back to CSV-only mode): %s",
                exc,
            )
            ManualQAService.set_global_rag_runtime(None)

    app.state.agent_pipeline = AgentPipeline()
    try:
        yield
    finally:
        del app.state.agent_pipeline
        ManualQAService.set_global_rag_runtime(None)
        MaterialEventPublisher.shutdown_executor(wait=True)


def create_app() -> FastAPI:
    """Create the FastAPI application."""

    app = FastAPI(title="Factory Space Backend", lifespan=lifespan)

    @app.get("/health")
    async def health() -> dict[str, str]:
        return {"status": "ok"}

    app.include_router(agent_connection_router)
    app.include_router(material_generation_router, prefix="/api/v1")
    app.include_router(debug_router, prefix="/api/v1")
    app.include_router(docs_router)
    app.include_router(manual_qa_docs_router)
    app.include_router(websocket_router)
    return app
