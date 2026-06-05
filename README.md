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

### 프로덕션(Prod) 서버 실행

프로덕션 설정 파일(`.env.prod`)을 로드하여 서버를 실행하려면 `run_prod_server.py` 스크립트를 사용합니다.

```bash
cd backend
uv run python scripts/run_prod_server.py
```

이 스크립트는 `.env.prod` 환경설정 파일을 로드하며, 로컬 LLM 슬롯이 설정되어 있을 경우 Ollama 서버 기동 여부를 확인하고 백그라운드로 안전하게 구동한 뒤 Uvicorn 프로덕션 서버를 시작합니다.
