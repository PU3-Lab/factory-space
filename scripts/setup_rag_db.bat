@echo off
chcp 65001 > nul
setlocal

set "DIR=%~dp0"
set "BACKEND_DIR=%DIR%..\backend"

echo [RAG Setup] Preparing PostgreSQL/pgvector RAG database...
pushd "%BACKEND_DIR%" || exit /b 1

echo [RAG Setup] Starting Docker compose postgres service...
docker compose -f docker-compose.rag.yml up -d postgres
if errorlevel 1 (
    echo [RAG Setup] Failed to start Docker postgres service.
    popd
    exit /b 1
)

echo [RAG Setup] Applying database migrations...
uv run --env-file .env.prod alembic upgrade head
if errorlevel 1 (
    echo [RAG Setup] Alembic migration failed.
    popd
    exit /b 1
)

echo [RAG Setup] Ingesting manual RAG documents...
uv run --env-file .env.prod python scripts/ingest_manual_rag.py
if errorlevel 1 (
    echo [RAG Setup] Manual RAG ingestion failed.
    popd
    exit /b 1
)

echo [RAG Setup] Verifying manual_rag_documents count...
uv run --env-file .env.prod python -c "from sqlalchemy import create_engine, text; import os; e=create_engine(os.getenv('FACTORY_DATABASE_URL')); c=e.connect(); print('manual_rag_documents=', c.execute(text('select count(*) from manual_rag_documents')).scalar())"
if errorlevel 1 (
    echo [RAG Setup] Verification failed.
    popd
    exit /b 1
)

echo [RAG Setup] Done.
popd
endlocal
