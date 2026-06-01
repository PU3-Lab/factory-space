# 세션 요약 - 2026-06-01

## 현재 상태

- 작업 디렉터리: `/Users/kimkyungpyo/Workspaces/projests/factory-space`
- 현재 브랜치: `feat/fastapi-agent-pipeline-connection`
- 원격 추적: 없음. 기준 branch는 `main`/`origin/main`.
- 최신 커밋: `67a8f83 Merge pull request #44 from PU3-Lab/feature/fastapi-lifespan-runner`
- 기준 상태: `feat/fastapi-agent-pipeline-connection`은 `main`에서 분기했다.
- 현재 미커밋 변경: FastAPI agent connection manifest route, WebSocket correlation contract test, smoke runner manifest check, README/plan/session summary.

## 프로젝트 구조

- `backend/`: Python 3.12+ FastAPI/LangGraph 기반 agent backend.
- `frontend/`: Unreal Engine 프로젝트 `Wanted_Factory`.
- `_workspace/factory-agent/`: 작업 규칙, RED artifact, 리뷰 피드백, 실수 기록.
- `.github/workflows/unreal-build.yml`: Windows self-hosted runner 기반 Unreal 빌드 확인 workflow.

## 최근 반영된 주요 작업

### FastAPI lifespan/server runner 정리

- PR #44 `feature/fastapi-lifespan-runner`가 `main`에 merge됨.
- `backend/scripts/run_server.py`가 추가되어 local FastAPI server 실행 경로가 명확해짐.
- 기본 backend 실행 포트는 Windows 충돌 가능성을 피하기 위해 `18000` 기준으로 문서화됨.
- `backend/src/app.py`, WebSocket gateway, smoke runner, 관련 테스트가 lifespan 실행 경로에 맞춰 정리됨.

### Unreal self-hosted runner workflow

- `.github/workflows/unreal-build.yml`이 존재함.
- `frontend/**` 변경이 `main` 또는 `dev`에 push/PR 될 때 Windows self-hosted runner에서 Unreal project file 생성과 editor target build를 수행하도록 구성됨.
- `self-hosted-runner-guide.md`에 Windows runner 설치, GitHub 등록, 서비스 실행 절차가 정리되어 있음.
- `task.md`와 `walkthrough.md`는 Unreal build workflow 구축 작업이 완료됐다고 기록함.

### Conveyor/Unreal frontend 반영

- PR #43 `LC_Conveyor`가 `main`에 merge됨.
- `frontend/Content/`에 conveyor, dummy grid/player/build controller 관련 asset이 추가됨.
- `frontend/Source/Wanted_Factory/Private`와 `Public` 아래에 dummy conveyor/grid/player/build controller C++ 파일들이 추가됨.

### Agent backend 현황

- backend는 FastAPI WebSocket endpoint와 LangGraph 기반 agent pipeline 구조를 가진다.
- Unreal 클라이언트 discovery용 `GET /api/v1/agent-connection` route를 추가하는 작업이 진행 중이다.
- 새 route는 `/health`, `/ws/agent`, 지원 top-level/leaf agent id, 샘플 `agent.request` envelope를 반환한다.
- 주요 agent 계층은 server orchestrator, domain orchestrator, leaf agent, common pipeline으로 구분한다.
- `backend/src/DECISION_LOG.md`에 pipeline, orchestrator, operator guide, quest generator, LangGraph 적용 범위와 책임 경계가 기록되어 있음.
- smoke runner는 `none`, `local`, `providers` profile을 가진다. `none` profile은 `/health`, `/api/v1/agent-connection`, `/ws/agent` error path를 확인한다. 외부 provider smoke는 `FACTORY_SMOKE_EXTERNAL_PROVIDER=1` opt-in일 때만 실행한다.

## 검증 상태

- 현재 세션에서 실행한 검증:
  - `backend/`에서 `uv run --extra dev pytest tests/test_agent_connection_router.py -q` → `2 passed`
  - `backend/`에서 `uv run --extra dev pytest tests/test_agent_connection_router.py tests/test_websocket_endpoint.py -q` → `7 passed`
  - `backend/`에서 `uv run --extra dev pytest tests/test_smoke_agent_pipeline_script.py -q` → `9 passed`
  - `backend/`에서 `uv run --extra dev pytest -q` → `117 passed`
  - `backend/`에서 `uv run ruff check .` → passed
  - `backend/`에서 `uv run --env-file smoke-none.env.example python scripts/run_server.py --port 18003` 실행 후 `uv run --env-file smoke-none.env.example python scripts/smoke_agent_pipeline.py none --base-url http://127.0.0.1:18003` → health/manifest/WebSocket smoke passed
- 최근 기록상 backend 검증 명령은 다음 기준을 사용한다.
  - `backend/`에서 `uv run --extra dev ruff check .`
  - `backend/`에서 `uv run --extra dev pytest -q`
  - 필요 시 `uv run --env-file smoke-none.env.example python scripts/smoke_agent_pipeline.py none`
- Unreal build는 GitHub Actions self-hosted Windows runner 또는 Windows Unreal 개발 환경에서 확인해야 한다.

## 남아 있는 원격 브랜치

- `origin/LDJ_UI`
- `origin/OJJ`
- `origin/SSR`
- `origin/chore/backend-restructure`
- `origin/codex/add-smoke-scenario-harness`
- `origin/codex/quest-agent-service`
- `origin/docs/korean-agents-md`
- `origin/feat-langgraph-orchestrator`
- `origin/fix-docs-links-and-uri`
- `origin/revert/pr-6`

## 다음 작업 전 확인 규칙

- 작업 시작 전에 `_workspace/factory-agent/compound.md`를 확인한다.
- 동작이나 코드 변경은 production edit 전에 RED artifact를 먼저 만든다.
- 함수 내부 import는 금지한다. import는 파일 상단에 둔다.
- 일반 source 파일은 500줄 이하로 유지한다. 넘기면 기능 의미 단위로 분리한다.
- Agent 선택은 ad hoc parsing logic이 아니라 prompt/structured-output과 LangGraph conditional routing으로 처리한다.
- 완료 보고나 커밋 전에 reviewer sub-agent를 사용한다.
- 리뷰 결과는 `_workspace/factory-agent/review-feedback-YYYY-MM-DD.md`처럼 날짜별로 남긴다.
- reviewer finding을 수정했다면 같은 범위로 재리뷰하고, unresolved finding이 없어질 때까지 반복한다.
- 서버, pipeline, WebSocket처럼 실제 실행 경로 확인이 가능한 단계라면 unit test와 함께 smoke test 또는 smoke script도 추가한다.

## 다음 작업 후보

- 현재 `main` 기준에서 필요한 신규 작업을 먼저 특정한다.
- backend 작업이면 관련 RED artifact를 `_workspace/factory-agent/`에 만들고 실패 확인 후 구현한다.
- Unreal/frontend 작업이면 Windows Unreal 환경 또는 self-hosted runner에서 빌드 검증 가능한 경로를 먼저 정한다.
