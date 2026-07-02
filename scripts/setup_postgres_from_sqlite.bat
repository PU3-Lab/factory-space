@echo off
chcp 65001 > nul
setlocal EnableDelayedExpansion

set "DIR=%~dp0"
set "BACKEND_DIR=%DIR%..\backend"
set "DB_URL=postgresql+psycopg://factory_space:factory_space@127.0.0.1:5433/factory_space?connect_timeout=5"

echo [Factory DB Setup] Preparing PostgreSQL/pgvector from factory_space.db...
pushd "%BACKEND_DIR%" || exit /b 1

if not exist "factory_space.db" (
    echo [Factory DB Setup] backend\factory_space.db was not found.
    echo [Factory DB Setup] Put factory_space.db in the backend folder and run this again.
    popd
    exit /b 1
)

where docker > nul 2> nul
if errorlevel 1 (
    echo [Factory DB Setup] Docker was not found in PATH.
    popd
    exit /b 1
)

where uv > nul 2> nul
if errorlevel 1 (
    echo [Factory DB Setup] uv was not found in PATH.
    popd
    exit /b 1
)

set "DATABASE_URL=%DB_URL%"
set "FACTORY_DATABASE_URL=%DB_URL%"

echo [Factory DB Setup] Starting Docker compose PostgreSQL service...
docker compose -f docker-compose.rag.yml up -d postgres
if errorlevel 1 (
    echo [Factory DB Setup] Failed to start Docker compose service.
    popd
    exit /b 1
)

echo [Factory DB Setup] Waiting for PostgreSQL health check...
for /L %%i in (1,1,30) do (
    set "DB_HEALTH="
    for /f "tokens=* usebackq" %%s in (`docker inspect -f "{{.State.Health.Status}}" factory-space-rag-postgres 2^>nul`) do set "DB_HEALTH=%%s"
    if "!DB_HEALTH!"=="healthy" goto db_ready
    timeout /t 2 /nobreak > nul
)

echo [Factory DB Setup] PostgreSQL did not become healthy in time.
popd
exit /b 1

:db_ready
echo [Factory DB Setup] PostgreSQL is healthy.

echo [Factory DB Setup] Applying Alembic migrations...
uv run alembic upgrade head
if errorlevel 1 (
    echo [Factory DB Setup] Alembic migration failed.
    popd
    exit /b 1
)

if "%RESET_POSTGRES_DATA%"=="1" (
    echo [Factory DB Setup] RESET_POSTGRES_DATA=1, existing copied data can be replaced.
) else (
    echo [Factory DB Setup] Existing different PostgreSQL data will be kept. Set RESET_POSTGRES_DATA=1 to replace it.
)

echo [Factory DB Setup] Copying SQLite data into PostgreSQL and updating local env files...
uv run python scripts\copy_sqlite_to_postgres.py --source factory_space.db --write-env .env --write-env .env.dev
if errorlevel 1 (
    echo [Factory DB Setup] SQLite to PostgreSQL copy failed.
    popd
    exit /b 1
)

echo [Factory DB Setup] Verifying PostgreSQL row counts...
uv run python scripts\copy_sqlite_to_postgres.py --source factory_space.db
if errorlevel 1 (
    echo [Factory DB Setup] Verification failed.
    popd
    exit /b 1
)

echo [Factory DB Setup] Done. The backend now points to local PostgreSQL/pgvector.
popd
endlocal
