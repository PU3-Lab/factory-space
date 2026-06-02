# FastAPI Agent Pipeline Connection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the FastAPI backend expose and verify a stable Unreal-to-agent-pipeline connection contract without changing the existing `/ws/agent` protocol.

**Architecture:** Keep `AgentPipeline` as an app-scoped dependency initialized in FastAPI lifespan. Keep `/ws/agent` as the only runtime agent transport. Add a small HTTP connection manifest router so Unreal can discover the backend health path, WebSocket path, supported agent ids, and a valid sample request before opening the WebSocket.

**Tech Stack:** FastAPI, Pydantic, LangGraph-backed `AgentPipeline`, pytest, FastAPI `TestClient`, existing `uv` backend workflow.

---

## Current Evidence

- `backend/src/app.py` already initializes `app.state.agent_pipeline = AgentPipeline()` in lifespan.
- `backend/src/websocket_gateway/gateway.py` already accepts `/ws/agent`, parses JSON, and sends `pipeline.run(message)` back to the client.
- `backend/tests/test_websocket_endpoint.py` already verifies `/health`, invalid JSON, invalid envelope, and `ROUTING_UNAVAILABLE`.
- Unreal does not need a new backend `ping` message type. Server liveness should use HTTP `/health`; WebSocket compatibility should use the existing `agent.request` envelope.

## File Structure

- Modify: `backend/src/app.py`
  - Include a new FastAPI router for connection metadata.
- Create: `backend/src/agent_connection/__init__.py`
  - Package marker for FastAPI connection metadata.
- Create: `backend/src/agent_connection/router.py`
  - Defines `GET /api/v1/agent-connection`.
  - Returns health path, WebSocket path, supported top-level agents, leaf-agent map, and a sample `agent.request`.
- Test: `backend/tests/test_agent_connection_router.py`
  - Verifies the manifest shape and supported agent ids.
- Modify: `backend/tests/test_websocket_endpoint.py`
  - Adds one contract test proving FastAPI WebSocket preserves `request_id`, `session_id`, `client_id`, and requested `agent` when routing is unavailable.
- Modify: `backend/README.md`
  - Documents the Unreal connection flow: `/health` first, then `/api/v1/agent-connection`, then `/ws/agent`.
- Optional after implementation: update `SESSION_SUMMARY.md`
  - Record the new route and verification commands.

## Non-Goals

- Do not add `ping` / `pong` protocol support.
- Do not add a second HTTP endpoint that runs the agent pipeline.
- Do not move `AgentPipeline` out of FastAPI lifespan.
- Do not change `agent.request`, `agent.response`, or `agent.error` envelope shapes.
- Do not implement Unreal C++ client code in this backend plan.

---

### Task 1: Lock Current WebSocket Pipeline Contract

**Files:**
- Modify: `backend/tests/test_websocket_endpoint.py`

- [x] **Step 1: Write the failing or characterization test**

Append this test to `backend/tests/test_websocket_endpoint.py`:

```python
def test_agent_websocket_preserves_unreal_correlation_fields_on_error() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "unreal-smoke-1",
                    "session_id": "dev-session",
                    "client_id": "unreal-client",
                    "agent": "process_optimizer",
                    "payload": {"machines": [{"id": "assembler-1"}]},
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["request_id"] == "unreal-smoke-1"
    assert response["session_id"] == "dev-session"
    assert response["client_id"] == "unreal-client"
    assert response["agent"] == "process_optimizer"
    assert response["error"]["code"] == "ROUTING_UNAVAILABLE"
```

- [x] **Step 2: Run the focused WebSocket tests**

Run:

```bash
cd backend
uv run --extra dev pytest tests/test_websocket_endpoint.py -q
```

Expected:

```text
5 passed
```

If this fails, fix only the correlation-preservation path in `backend/src/agents/pipeline/runtime.py` or `backend/src/agents/pipeline/utils.py`. Do not change the public envelope shape.

- [ ] **Step 3: Commit after the test is passing**

```bash
git add backend/tests/test_websocket_endpoint.py
git commit -m "test: lock unreal websocket correlation contract"
```

---

### Task 2: Add FastAPI Agent Connection Manifest

**Files:**
- Create: `backend/src/agent_connection/__init__.py`
- Create: `backend/src/agent_connection/router.py`
- Modify: `backend/src/app.py`
- Test: `backend/tests/test_agent_connection_router.py`

- [x] **Step 1: Write the failing manifest tests**

Create `backend/tests/test_agent_connection_router.py`:

```python
from __future__ import annotations

from fastapi.testclient import TestClient

from app import create_app


def test_agent_connection_manifest_exposes_unreal_connection_contract() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/api/v1/agent-connection")

    assert response.status_code == 200
    body = response.json()
    assert body["status"] == "ok"
    assert body["health_path"] == "/health"
    assert body["websocket_path"] == "/ws/agent"
    assert body["request_type"] == "agent.request"
    assert body["response_types"] == ["agent.response", "agent.error"]
    assert body["sample_request"] == {
        "type": "agent.request",
        "request_id": "unreal-smoke-1",
        "session_id": "dev-session",
        "client_id": "unreal-client",
        "agent": "process_optimizer",
        "payload": {"machines": [{"id": "assembler-1"}]},
    }


def test_agent_connection_manifest_lists_supported_agent_ids() -> None:
    with TestClient(create_app()) as client:
        body = client.get("/api/v1/agent-connection").json()

    assert body["top_level_agents"] == [
        "process_optimizer",
        "operator_guide",
        "quest_generator",
        "new_material_generator",
    ]
    assert body["leaf_agents"] == {
        "process_optimizer": ["process_optimizer"],
        "operator_guide": [
            "operator_guide.recipe_explainer",
            "operator_guide.machine_help",
            "operator_guide.troubleshooter",
        ],
        "quest_generator": [
            "quest_generator.tutorial_quest",
            "quest_generator.production_quest",
            "quest_generator.exploration_quest",
            "quest_generator.economy_quest",
        ],
        "new_material_generator": ["new_material_generator"],
    }
```

- [x] **Step 2: Run the manifest tests to verify RED**

Run:

```bash
cd backend
uv run --extra dev pytest tests/test_agent_connection_router.py -q
```

Expected:

```text
FAILED ... assert 404 == 200
```

- [x] **Step 3: Add the router package marker**

Create `backend/src/agent_connection/__init__.py`:

```python
"""FastAPI routes exposing agent connection metadata."""
```

- [x] **Step 4: Implement the connection manifest router**

Create `backend/src/agent_connection/router.py`:

```python
"""FastAPI connection manifest for Unreal agent clients."""

from __future__ import annotations

from fastapi import APIRouter

from agents.operator_guide.agent import OPERATOR_GUIDE_LEAF_AGENT_IDS
from agents.orchestrator import TOP_LEVEL_AGENT_IDS
from agents.pipeline.graph_edges import SINGLE_LEAF_AGENT_IDS
from agents.quest_generator.agent import QUEST_SUB_AGENT_IDS

router = APIRouter(prefix="/api/v1/agent-connection", tags=["agent-connection"])


@router.get("")
async def get_agent_connection_manifest() -> dict[str, object]:
    """Return the stable backend connection contract for Unreal clients."""

    return {
        "status": "ok",
        "health_path": "/health",
        "websocket_path": "/ws/agent",
        "request_type": "agent.request",
        "response_types": ["agent.response", "agent.error"],
        "top_level_agents": list(TOP_LEVEL_AGENT_IDS),
        "leaf_agents": {
            "process_optimizer": list(SINGLE_LEAF_AGENT_IDS["process_optimizer"]),
            "operator_guide": list(OPERATOR_GUIDE_LEAF_AGENT_IDS),
            "quest_generator": list(QUEST_SUB_AGENT_IDS),
            "new_material_generator": list(
                SINGLE_LEAF_AGENT_IDS["new_material_generator"]
            ),
        },
        "sample_request": {
            "type": "agent.request",
            "request_id": "unreal-smoke-1",
            "session_id": "dev-session",
            "client_id": "unreal-client",
            "agent": "process_optimizer",
            "payload": {"machines": [{"id": "assembler-1"}]},
        },
    }
```

- [x] **Step 5: Include the router in the FastAPI app**

Modify `backend/src/app.py`:

```python
from agent_connection.router import router as agent_connection_router
from agents.pipeline import AgentPipeline
from websocket_gateway.gateway import router as websocket_router
```

Then include the router before or after the WebSocket router:

```python
    app.include_router(agent_connection_router)
    app.include_router(websocket_router)
    return app
```

- [x] **Step 6: Run the manifest tests to verify GREEN**

Run:

```bash
cd backend
uv run --extra dev pytest tests/test_agent_connection_router.py -q
```

Expected:

```text
2 passed
```

- [x] **Step 7: Run the WebSocket tests again**

Run:

```bash
cd backend
uv run --extra dev pytest tests/test_agent_connection_router.py tests/test_websocket_endpoint.py -q
```

Expected:

```text
7 passed
```

- [ ] **Step 8: Commit**

```bash
git add backend/src/agent_connection backend/src/app.py backend/tests/test_agent_connection_router.py
git commit -m "feat: expose agent connection manifest"
```

---

### Task 3: Document Unreal Connection Flow

**Files:**
- Modify: `backend/README.md`

- [x] **Step 1: Add the FastAPI connection flow section**

In `backend/README.md`, under the existing WebSocket section, add:

```markdown
### Unreal 클라이언트 연결 순서

Unreal 클라이언트는 서버 생존 확인과 Agent WebSocket 통신 확인을 분리합니다.

1. `GET http://127.0.0.1:18000/health`
   - 기대 응답: `{"status":"ok"}`
   - 이 요청은 서버 프로세스 생존 여부만 확인합니다.
2. `GET http://127.0.0.1:18000/api/v1/agent-connection`
   - 기대 응답: WebSocket path, 지원 agent id, sample `agent.request`
   - Unreal 쪽 connection router가 이 값을 기준으로 요청 payload를 구성합니다.
3. `ws://127.0.0.1:18000/ws/agent`
   - 실제 Agent 요청은 WebSocket으로 보냅니다.
   - local LLM이나 외부 provider가 없으면 `ROUTING_UNAVAILABLE` error가 올 수 있습니다.
   - 이 경우에도 request/response JSON이 정상 왕복했다면 transport 통신은 성공입니다.
```

- [x] **Step 2: Run markdown whitespace validation**

Run:

```bash
git diff --check -- backend/README.md
```

Expected:

```text
no output
```

- [ ] **Step 3: Commit**

```bash
git add backend/README.md
git commit -m "docs: document unreal agent connection flow"
```

---

### Task 4: Verify End-to-End Backend Contract

**Files:**
- No production file changes expected.

Note: commit steps remain unchecked in this session because no commit was requested.

- [x] **Step 1: Run backend unit tests**

Run:

```bash
cd backend
uv run --extra dev pytest tests/test_agent_connection_router.py tests/test_websocket_endpoint.py tests/test_smoke_agent_pipeline_script.py -q
```

Expected:

```text
all selected tests pass
```

- [x] **Step 2: Run lint**

Run:

```bash
cd backend
uv run --extra dev ruff check .
```

Expected:

```text
All checks passed!
```

- [x] **Step 3: Run smoke with a local server**

Terminal 1:

```bash
cd backend
uv run --env-file smoke-none.env.example python scripts/run_server.py --no-reload
```

Terminal 2:

```bash
cd backend
uv run --env-file smoke-none.env.example python scripts/smoke_agent_pipeline.py none
```

Expected:

```text
PASS none/health
PASS none/agent_connection_manifest
PASS none/invalid_json
PASS none/invalid_envelope
PASS none/routing_unavailable
```

- [x] **Step 4: Manually check the manifest**

Run while the server is up:

```bash
curl http://127.0.0.1:18000/api/v1/agent-connection
```

Expected response contains:

```json
{
  "status": "ok",
  "health_path": "/health",
  "websocket_path": "/ws/agent",
  "request_type": "agent.request"
}
```

- [ ] **Step 5: Final commit if verification changed docs or summaries**

If only verification ran, do not create another commit. If `SESSION_SUMMARY.md` or another tracking file was updated, commit it separately:

```bash
git add SESSION_SUMMARY.md
git commit -m "docs: update session summary"
```

---

## Self-Review

### Spec Coverage

- FastAPI scope is covered by `backend/src/app.py` and the new `agent_connection` FastAPI router.
- Agent pipeline connection is covered by preserving existing `/ws/agent -> AgentPipeline.run()` behavior.
- Unreal client preflight is covered by `/health` and `/api/v1/agent-connection`.
- WebSocket contract validation is covered by `test_websocket_endpoint.py`.
- Backend-only scope is preserved; Unreal C++ implementation is not included.

### Placeholder Scan

- No `TBD`, `TODO`, `implement later`, or vague "add validation" steps are used.
- Each task lists exact files, exact commands, and expected outcomes.

### Type Consistency

- Route path is consistently `/api/v1/agent-connection`.
- WebSocket path is consistently `/ws/agent`.
- Request envelope remains `agent.request`.
- Response envelope types remain `agent.response` and `agent.error`.
