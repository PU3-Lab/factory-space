# Process Optimizer v2 Sprint 5 코드리뷰

## 검토 대상

- 기획 문서: `docs/process_optimizer/process_optimizer_v2_sprint_5_plan.md`
- 구현 범위: 승인된 변경 항목을 Unreal 명령 payload로 변환하고, 실행 준비 단계에서 실행 기록을 남기는 흐름
- 주요 파일:
  - `backend/src/agents/process_optimizer/commands.py`
  - `backend/src/agents/process_optimizer/execution_record.py`
  - `backend/src/agents/process_optimizer/nodes.py`
  - `backend/src/agents/process_optimizer/graph.py`
  - `backend/src/agents/process_optimizer/graph_state.py`

## 구현 확인

Sprint 5의 핵심 흐름은 구현되어 있다.

```text
validate_selected_changes
-> build_unreal_commands
-> validate_command_payloads
-> create_execution_record
-> return_command_payload
```

확인된 동작은 다음과 같다.

- 승인된 change만 `commands`로 변환한다.
- 허용 명령 schema를 통과하지 못하면 `invalid_command_payload`로 차단한다.
- `plan_id + change_id` 기준으로 중복 실행을 차단한다.
- 최종 응답은 `status: execute_ready`와 `commands`를 포함한다.

## 보완한 부분

### 1. `context.factoryRevision` 지원 복구

Sprint 4에서 보완했던 WebSocket envelope 호환성이 Sprint 5 구현 중 빠져 있었다.

수정 후에는 `payload.factoryRevision`이 없으면 `context.factoryRevision`을 사용한다.

```text
Before: payload.factoryRevision만 확인
After : payload.factoryRevision -> context.factoryRevision 순서로 확인
```

### 2. 빈 선택 목록 차단

`approved_change_ids: []`가 들어오면 실행할 변경이 없으므로 명확히 실패해야 한다.

수정 후에는 `no_changes_selected`로 응답한다.

```text
Before: 빈 목록이 조용히 통과할 수 있음
After : no_changes_selected 오류로 차단
```

### 3. 실행 기록의 before/after 추정값 제거

Sprint 5 단계에서는 Unreal의 실제 변경 전/후 상태를 아직 받지 않는다. 따라서 코드가 임의로 `disabled`, `connected`, `iron_ingot` 같은 값을 실제 상태처럼 기록하면 Sprint 6 이후 undo 판단이 위험해진다.

수정 후 실행 기록은 아래처럼 명시적으로 “Unreal 확인 필요” 상태로 저장한다.

```text
before.source = unreal_runtime_required
after.source = planned_command
after.requires_unreal_confirmation = true
```

즉, 현재 기록은 실제 월드 상태가 아니라 “승인된 명령 준비 기록”임을 분명히 한다.

## 검증 결과

실행 명령:

```powershell
uv run --env-file .env.prod python -m py_compile src/agents/process_optimizer/graph_state.py src/agents/process_optimizer/nodes.py tests/test_process_optimizer_apply.py tests/test_process_optimizer_execution_record.py
uv run --env-file .env.prod pytest tests/test_process_optimizer_commands.py tests/test_process_optimizer_execution_record.py tests/test_process_optimizer_apply.py tests/test_process_optimizer_preview_store.py tests/test_process_optimizer_graph.py tests/test_process_optimizer.py tests/test_process_optimizer_prompt.py -q
```

결과:

```text
40 passed
```

## 남은 리스크

`build_command_payload`는 아직 `recommended_action` 텍스트를 기반으로 명령을 추론한다. Sprint 5 MVP로는 허용 가능하지만, 최종 구조에서는 분석/계획 단계에서 구조화된 command candidate를 만들고 Sprint 6~7에서 Unreal 검증 결과와 연결하는 편이 안전하다.

## 판단

Sprint 5는 기획 범위대로 구현되었고, Sprint 4에서 확보한 승인/리비전 안전장치도 다시 보존되었다. 다음 단계인 Sprint 6로 넘어가도 된다.
