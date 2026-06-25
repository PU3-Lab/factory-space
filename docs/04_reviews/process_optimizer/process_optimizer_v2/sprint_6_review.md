# Process Optimizer v2 Sprint 6 코드리뷰

## 검토 대상

- 기획 문서: `docs/process_optimizer/process_optimizer_v2_sprint_6_plan.md`
- 구현 범위: undo 요청, 실행 기록 조회, 현재 상태와 recorded after 비교, 충돌 차단, inverse command 생성
- 주요 파일:
  - `backend/src/agents/process_optimizer/undo.py`
  - `backend/src/agents/process_optimizer/nodes.py`
  - `backend/src/agents/process_optimizer/graph.py`
  - `backend/src/agents/process_optimizer/schemas.py`
  - `backend/src/agents/pipeline/graph_edges.py`
  - `backend/src/agents/pipeline/runtime.py`
  - `backend/tests/test_process_optimizer_undo.py`

## 검토 결과

Sprint 6의 전용 graph 내부 undo 흐름은 구현되어 있었다. 다만 최초 검토 시 실제 Unreal WebSocket 경로인 `AgentPipeline`에는 `apply/undo`가 연결되어 있지 않았고, `apply` operation이 schema에서 빠져 있었으며, before 상태를 모르는 경우 inverse command가 임의 기본값으로 만들어질 수 있었다.

이번 보완으로 graph 내부 테스트뿐 아니라 public pipeline 경로에서도 `apply/undo`가 v2 graph를 타도록 수정했다.

## 보완한 부분

### 1. public pipeline에서 `apply/undo` 라우팅 연결

`route_process_optimizer()`가 기존에는 `state_update`, `analyze`만 처리했다. 그래서 `operation: "undo"` 요청은 `ROUTING_UNAVAILABLE`로 빠졌다.

수정 후 흐름:

```text
validate_process_payload
-> route_process_optimizer
-> process_optimizer_v2_graph
-> build_agent_response
```

`analyze`는 기존 v1 분석/제안 경로를 유지하고, `apply/undo`만 v2 graph로 보낸다.

### 2. schema에 `apply` operation 추가

`ProcessOptimizerPayload.operation`에 `apply`를 추가했다.

```text
Before: state_update, analyze, undo
After : state_update, analyze, apply, undo
```

### 3. undo error status 응답 허용

`ProcessOptimizerResponse.status`에 undo 흐름에서 실제로 반환되는 status를 추가했다.

```text
record_not_found
invalid_factory_state
undo_conflict
```

### 4. apply 요청의 `factory_state`를 execution record까지 전달

`validate_apply_request()`가 `payload.factory_state`를 graph state에 복사하도록 보완했다. 이제 apply 시점에 Unreal이 최신 상태를 함께 보내면 `create_execution_record()`가 실제 before 상태를 기록할 수 있다.

### 5. before 상태를 모르면 inverse command 생성 차단

Sprint 5 기록처럼 `before.state_known == false`인 경우, undo가 `copper_ingot`, `enabled=true` 같은 기본값을 추정하지 않도록 막았다.

```text
before.state_known == false
-> build_inverse_command() returns None
-> undo_conflict
-> commands: []
```

이제 undo는 실제 before 값이 있거나 명시적인 inverse command가 기록된 경우에만 복구 명령을 만든다.

## 검증 결과

실행 명령:

```powershell
uv run --env-file .env.prod python -m py_compile src/agents/pipeline/runtime.py src/agents/pipeline/graph_edges.py src/agents/process_optimizer/schemas.py src/agents/process_optimizer/nodes.py src/agents/process_optimizer/undo.py tests/test_process_optimizer.py tests/test_process_optimizer_apply.py tests/test_process_optimizer_undo.py
uv run --env-file .env.prod pytest tests/test_process_optimizer_undo.py tests/test_process_optimizer_apply.py tests/test_process_optimizer_commands.py tests/test_process_optimizer_execution_record.py tests/test_process_optimizer_preview_store.py tests/test_process_optimizer_graph.py tests/test_process_optimizer.py tests/test_process_optimizer_prompt.py -q
```

결과:

```text
47 passed
```

추가된 검증:

- `ProcessOptimizerPayload`가 `operation: "apply"`를 허용한다.
- `AgentPipeline.run()`에서 `apply`가 v2 graph로 라우팅된다.
- `AgentPipeline.run()`에서 `undo`가 v2 graph로 라우팅된다.
- apply 요청에 포함된 `factory_state`가 execution record의 authoritative before 값으로 기록된다.
- before 상태가 unknown이면 undo command를 만들지 않고 `undo_conflict`로 차단한다.

## 남은 리스크

현재 `analyze`는 기존 v1 분석/제안 경로를 유지하고, `apply/undo`만 v2 graph에 연결되어 있다. 이는 v2 점진 이전 전략과 맞지만, Sprint 7 이후 measurement까지 붙인 뒤에는 analyze도 v2 graph로 완전히 이전할지 결정해야 한다.

또한 `connect_conveyor` 계열 command는 Unreal 쪽 실제 연결 상태 schema가 더 구체화되면 conflict 비교 로직을 보강하는 것이 좋다.

## 판단

Sprint 6은 기획 의도에 맞게 보완되었고, public WebSocket pipeline 기준으로도 `apply/undo` 진입 경로가 확인되었다. 다음 단계인 Sprint 7 measurement 구현으로 넘어가도 된다.
