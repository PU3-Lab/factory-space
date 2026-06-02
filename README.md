# factory-space

## 서버 실행법

백엔드 서버는 `backend` 디렉터리에서 `uv`로 실행합니다.

```bash
cd backend
uv sync
uv run python scripts/run_server.py
```

기본 실행 주소는 다음과 같습니다.

```text
Health check: http://127.0.0.1:18000/health
Agent connection manifest: http://127.0.0.1:18000/api/v1/agent-connection
WebSocket: ws://127.0.0.1:18000/ws/agent
```

다른 포트로 실행하려면 `--port`를 지정합니다.

```bash
uv run python scripts/run_server.py --port 18001
```

외부 LLM 없이 서버와 WebSocket 경로만 확인하려면 `smoke-none.env.example`을 사용합니다.

```bash
uv run --env-file smoke-none.env.example python scripts/run_server.py
uv run --env-file smoke-none.env.example python scripts/smoke_agent_pipeline.py none
```
