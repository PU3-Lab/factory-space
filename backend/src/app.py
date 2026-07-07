"""ASGI application for the Factory Space backend."""

from __future__ import annotations

import logging
import os
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
from sqlalchemy import func, select

from agent_connection.router import router as agent_connection_router
from agents.material_generation.events import MaterialEventPublisher
from agents.material_generation.router import router as material_generation_router
from agents.operator_guide.debug_router import router as debug_router
from agents.operator_guide.multi_question_rag_retriever import MultiQuestionRagRetriever
from agents.operator_guide.rag_embedding import create_embedding_provider
from agents.operator_guide.rag_retriever import ManualRagRetriever
from agents.operator_guide.rag_store import create_manual_rag_store_from_env
from agents.operator_guide.service import ManualQAService
from agents.pipeline import AgentPipeline
from agents.quest_generator.quest_router import router as quest_router
from db.engine import get_db_session
from db.models import RecipeModel
from db.recipe_ingestion import ingest_recipes
from docs_router import router as docs_router
from manual_qa_docs.router import router as manual_qa_docs_router
from tts.router import tts_router
from websocket_gateway.gateway import router as websocket_router


def _load_env_file(env_file: Path) -> None:
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


def _load_backend_env() -> None:
    """Load the configured backend env file for direct ASGI app startup."""

    backend_root = Path(__file__).resolve().parents[1]
    configured_env_file = Path(os.environ.get("FACTORY_ENV_FILE") or ".env")
    env_file = (
        configured_env_file
        if configured_env_file.is_absolute()
        else backend_root / configured_env_file
    )
    _load_env_file(env_file)


def _auto_ingest_recipes_enabled() -> bool:
    value = os.environ.get("FACTORY_AUTO_INGEST_RECIPES", "").strip().lower()
    return value in {"1", "true", "yes", "on"}


def _count_recipes() -> int:
    with get_db_session() as session:
        return session.scalar(select(func.count(RecipeModel.id))) or 0


def _ingest_recipes_from_csv() -> None:
    ingest_recipes()


def _maybe_ingest_dev_recipes() -> None:
    if not _auto_ingest_recipes_enabled():
        return

    try:
        if _count_recipes() > 0:
            return
        _ingest_recipes_from_csv()
    except Exception as exc:
        logging.getLogger("app").warning(
            "Failed to auto-ingest recipes for development: %s",
            exc,
        )


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    """Initialize application-scoped runtime dependencies."""
    _load_backend_env()
    _maybe_ingest_dev_recipes()

    # Ensure the background executor is initialized and active for this app context
    MaterialEventPublisher.reset_executor(wait=False)

    # Initialize RAG runtime for operator_guide
    if os.environ.get("FACTORY_RAG_RUNTIME_MOCK") != "true":
        try:
            embedding_provider = create_embedding_provider()
            search_store = create_manual_rag_store_from_env()
            if search_store is None:
                raise RuntimeError(
                    "FACTORY_RAG_DATABASE_URL is required for operator_guide RAG runtime."
                )
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
    app.include_router(quest_router, prefix="/api/v1")
    app.include_router(debug_router, prefix="/api/v1")
    app.include_router(docs_router)
    app.include_router(manual_qa_docs_router)
    app.include_router(websocket_router)
    app.include_router(tts_router)

    _mount_material_assets(app)
    return app


def _mount_material_assets(app: FastAPI) -> None:
    """Serve generated material images (var/assets/materials) as static files.

    DB의 asset key가 ``materials/{id}/icon.png`` 형태이므로, 클라이언트는
    ``GET /materials/{id}/icon.png`` (= ``/`` + asset_key)로 이미지를 받는다.
    저장 경로는 visual_pipeline과 동일한 FACTORY_IMAGE_STORAGE_PATH를 따른다.
    """
    storage_base = os.environ.get("FACTORY_IMAGE_STORAGE_PATH", "var/assets")
    materials_dir = Path(storage_base) / "materials"
    # StaticFiles는 디렉터리가 없으면 마운트 시 에러를 내므로 미리 생성한다.
    materials_dir.mkdir(parents=True, exist_ok=True)
    app.mount(
        "/materials",
        StaticFiles(directory=str(materials_dir)),
        name="materials",
    )

