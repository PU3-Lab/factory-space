# Task RED - Agent pipeline smoke runner

## Scope

Agent pipeline smoke runner를 순서대로 구성한다.

1. 외부 API 없는 smoke
2. local LLM smoke
3. external provider smoke

## Contract

- smoke runner는 `backend/scripts/smoke_agent_pipeline.py`에 둔다.
- 기본 server URL은 `http://127.0.0.1:8000`이고 WebSocket URL은 `/ws/agent`다.
- `none` profile은 외부 API 없이 실행 가능해야 하며 `/health`, invalid JSON, wrong envelope, routing unavailable을 확인한다.
- `local` profile은 local LLM 서버가 붙은 backend를 대상으로 네 Agent 경로를 1개씩 확인한다.
- `providers` profile은 `FACTORY_SMOKE_EXTERNAL_PROVIDER=1`이 없으면 skip으로 종료하고, opt-in일 때 provider backend를 대상으로 대표 요청을 확인한다.
- CI 기본 unit test는 실제 server나 외부 API를 요구하지 않는다.

## RED verification

Production script 작성 전에 `backend/tests/test_smoke_agent_pipeline_script.py`를 먼저 추가한다.

예상 실패:

- `scripts.smoke_agent_pipeline` 모듈이 없어 import error가 발생한다.

실제 RED 결과:

- 실행: `uv run --extra dev pytest tests/test_smoke_agent_pipeline_script.py -q`
- 결과: collection error.
- 실패 이유: `scripts.smoke_agent_pipeline` 모듈이 아직 없어 `ImportError: cannot import name 'smoke_agent_pipeline' from 'scripts'`가 발생했다.

## Acceptance

- smoke profile별 case 목록과 expected response 검증이 unit test로 고정된다.
- script에는 함수 내부 import가 없다.
- `uv run --extra dev pytest tests/test_smoke_agent_pipeline_script.py -q`가 통과한다.
- `uv run --extra dev ruff check .`가 통과한다.

## GREEN verification

- 실행: `uv run --extra dev pytest tests/test_smoke_agent_pipeline_script.py -q`
- 결과: 7 passed.
- 실행: `uv run --extra dev ruff check .`
- 결과: All checks passed.
- 실행: `uv run --extra dev pytest -q`
- 결과: 95 passed.
- 실행: `uv run --env-file smoke-none.env.example uvicorn app:app --host 127.0.0.1 --port 18082`
- 실행: `uv run --env-file smoke-none.env.example python scripts/smoke_agent_pipeline.py none --base-url http://127.0.0.1:18082`
- 결과: `PASS none/health`, `PASS none/invalid_json`, `PASS none/invalid_envelope`, `PASS none/routing_unavailable`.
