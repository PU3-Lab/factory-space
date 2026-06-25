@echo off
chcp 65001 > nul
setlocal EnableDelayedExpansion

set "DIR=%~dp0"
set "BACKEND_DIR=%DIR%..\backend"

echo [Factory Stack] Starting RAG PostgreSQL/pgvector container...
pushd "%BACKEND_DIR%" || exit /b 1

docker compose -f docker-compose.rag.yml up -d postgres
if errorlevel 1 (
    echo [Factory Stack] Failed to start Docker compose service.
    popd
    exit /b 1
)

echo [Factory Stack] Waiting for PostgreSQL health check...
for /L %%i in (1,1,30) do (
    set "DB_HEALTH="
    for /f "tokens=* usebackq" %%s in (`docker inspect -f "{{.State.Health.Status}}" factory-space-rag-postgres 2^>nul`) do set "DB_HEALTH=%%s"
    if "!DB_HEALTH!"=="healthy" goto db_ready
    timeout /t 2 /nobreak > nul
)

echo [Factory Stack] PostgreSQL did not become healthy in time.
popd
exit /b 1

:db_ready
echo [Factory Stack] PostgreSQL is healthy.

echo [Factory Stack] Applying database migrations...
uv run --env-file .env.prod alembic upgrade head
if errorlevel 1 (
    echo [Factory Stack] Database migration failed.
    popd
    exit /b 1
)

if "%RUN_RAG_INGEST%"=="1" (
    echo [Factory Stack] RUN_RAG_INGEST=1, ingesting RAG documents...
    uv run --env-file .env.prod python scripts/ingest_manual_rag.py
    if errorlevel 1 (
        echo [Factory Stack] RAG ingestion failed.
        popd
        exit /b 1
    )
) else (
    echo [Factory Stack] Skipping RAG ingestion. Set RUN_RAG_INGEST=1 to ingest updated documents.
)

echo [Factory Stack] Starting production backend server...
uv run --env-file .env.prod python scripts/run_prod_server.py %*

popd
endlocal
