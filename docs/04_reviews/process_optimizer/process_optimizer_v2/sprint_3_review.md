# Process Optimizer v2 Sprint 3 코드리뷰

`process_optimizer` 전용 LangGraph의 preview 계획 임시 저장과 유효성 관리 구현을 검토한 문서입니다.

## 1. 검토 결과

Sprint 3의 핵심 목표인 `PreviewPlan` 생성, `(session_id, plan_id)` 기반 저장, 5분 유효기간, `factoryRevision` 충돌 검증 API는 구현되었습니다. 다만 리뷰 과정에서 실제 WebSocket envelope와 맞지 않는 `session_id` 저장 위치와 근거 없는 예상 개선율이 확인되어 보완했습니다.

## 2. 수정한 보완 사항

### 2.1 session_id 저장 기준 보정

기존 구현은 `session_id`를 payload 내부에서만 찾았습니다. 실제 `/ws/agent` envelope에서는 `session_id`가 top-level에 있으므로, preview 계획이 `default-session`으로 저장될 수 있었습니다.

수정 후 기준은 다음 순서입니다.

```text
1. top-level state.session_id
2. payload.session_id
3. payload.session-id
4. default-session
```

이로써 Sprint 4에서 apply 요청이 실제 session 기준으로 preview plan을 찾을 수 있는 기반이 생겼습니다.

### 2.2 임의 개선율 제거

기존 `create_preview_plan`에 다시 들어온 `operating_rate_improvement = len(suggestions) * 0.05`는 실제 측정 근거가 없는 값이었습니다. Sprint 3 preview 단계에서는 정량 개선율을 만들지 않고, 해결 대상 count만 제공합니다.

수정 후 `expected_effect`는 다음 형태입니다.

```json
{
  "estimated": false,
  "resolved_input_shortages_count": 1,
  "resolved_output_blocks_count": 0,
  "resolved_conveyor_congestions_count": 0
}
```

실제 개선율은 Sprint 7의 measurement 단계에서 계산하는 것이 맞습니다.

### 2.3 테스트 정리

깨진 인코딩의 테스트 docstring을 정리하고, 다음 회귀 검증을 추가했습니다.

```text
- top-level session_id로 preview plan이 저장되는지 검증
- payload session_id도 테스트 콘솔 호환용으로 지원되는지 검증
- expected_effect에 operating_rate_improvement가 없는지 검증
- preview plan 저장, 조회, 만료, revision 충돌, clear 동작 분리 검증
```

## 3. 구현 상태

| 항목 | 상태 | 비고 |
| --- | --- | --- |
| PreviewPlan schema | 완료 | plan_id, session_id, factoryRevision, goal, changes, expected_effect, ui_hints, created_at, expires_at 포함 |
| PreviewPlanStore | 완료 | 메모리 store, session_id + plan_id 복합 키 사용 |
| 5분 만료 검증 | 완료 | `is_expired` API 제공 |
| revision 충돌 검증 | 완료 | `check_revision_conflict` API 제공 |
| graph 저장 노드 | 완료 | `create_preview_plan -> save_preview_plan -> return_preview_plan` 연결 |
| apply graph 연결 | 다음 Sprint | Sprint 4에서 plan 조회, 만료, revision 충돌을 실행 흐름에 연결 예정 |

## 4. 개선 기록

포트폴리오에 사용할 수 있도록 이번 Sprint에서 개선된 값을 정리했습니다.

| 구분 | 변경 전 | 변경 후 |
| --- | --- | --- |
| preview 저장 키 | payload session_id 의존, 누락 시 default-session | top-level session_id 우선, payload는 호환 fallback |
| 근거 없는 개선율 필드 | `operating_rate_improvement` 1개 생성 | 0개 생성, `estimated: false`로 명시 |
| preview 유효 시간 | 응답에 저장 기준 없음 | 5분 TTL과 `expires_at` 저장 |
| revision 충돌 판단 | apply 단계에서 참조할 저장 API 없음 | `check_revision_conflict(plan, current_revision)` 제공 |
| graph preview 저장 단계 | preview 응답만 생성 | preview plan 생성, 저장, 응답까지 3단계 분리 |
| Sprint 3 회귀 테스트 | store/graph 혼합 검증 중심 | session 저장, 만료, 충돌, 임의 수치 제거까지 26개 테스트로 검증 |

## 5. 검증 결과

```text
uv run --env-file .env.prod pytest tests/test_process_optimizer_preview_store.py tests/test_process_optimizer_graph.py tests/test_process_optimizer.py tests/test_process_optimizer_prompt.py -q

26 passed
```

```text
uv run --env-file .env.prod python -m py_compile src/agents/process_optimizer/preview_store.py src/agents/process_optimizer/graph_state.py src/agents/process_optimizer/nodes.py src/agents/process_optimizer/graph.py tests/test_process_optimizer_preview_store.py tests/test_process_optimizer_graph.py

passed
```

## 6. 다음 단계

Sprint 4에서는 저장된 preview plan을 실제 `apply` 요청에서 조회해야 합니다. 이때 아래 순서로 막아야 합니다.

```text
1. plan_id 존재 여부 확인
2. session_id 일치 확인
3. expires_at 만료 여부 확인
4. factoryRevision 충돌 여부 확인
5. approval true 여부 확인
```

이 단계까지 연결되면 preview와 실행 흐름이 분리되고, 승인 없는 실행과 오래된 계획 실행을 안정적으로 차단할 수 있습니다.
