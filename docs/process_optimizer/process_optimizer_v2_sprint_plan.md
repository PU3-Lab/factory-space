# Process Optimizer v2 Sprint 계획

## 1. 목적

이 문서는 `process_optimizer_v2_langgraph_plan.md`를 실제 구현 가능한 sprint 단위로 나누기 위한 계획서다.

현재 구현된 v1은 분석/제안 MVP로 유지하고, v2는 전용 LangGraph를 단계적으로 추가한다.

## 2. 전체 Sprint 요약

| Sprint | 목표 | 핵심 산출물 |
| --- | --- | --- |
| V2-1 | 전용 LangGraph 뼈대 생성 | `graph_state.py`, `nodes.py`, `graph.py` |
| V2-2 | analyze -> preview 이전 | preview plan 응답, schema 검증 |
| V2-3 | preview 저장과 유효성 관리 | `plan_id`, `expires_at`, `factoryRevision` 검증 |
| V2-4 | apply 승인 흐름 | approval guard, selected changes 검증 |
| V2-5 | Unreal command와 execution record | command payload, 실행 기록 저장 |
| V2-6 | undo 충돌 검증 | inverse command, conflict response |
| V2-7 | 효과 측정과 재분석 | before/after metrics, measurement response |
| V2-8 | 통합 smoke test와 문서 최신화 | WebSocket sample, demo guide, review 문서 |

## 3. Sprint V2-1: 전용 LangGraph 뼈대

### 목표

`process_optimizer` 전용 graph 구조를 만든다. 이 sprint에서는 실제 최적화 로직을 모두 옮기지 않고, graph가 입력을 받아 안전한 기본 preview 응답을 반환하는 것까지만 구현한다.

### 작업

```text
- backend/src/agents/process_optimizer/graph_state.py 생성
- backend/src/agents/process_optimizer/nodes.py 생성
- backend/src/agents/process_optimizer/graph.py 생성
- ProcessOptimizerGraphState 정의
- 최소 node 함수 정의
- graph compile 함수 작성
```

### 성공 기준

```text
- graph.py가 import 가능하다.
- 기본 analyze 요청을 graph에 넣으면 preview 형태의 기본 응답이 나온다.
- 기존 v1 테스트는 깨지지 않는다.
```

### 테스트

```text
backend/tests/test_process_optimizer_graph.py
```

## 4. Sprint V2-2: analyze -> preview 이전

### 목표

현재 v1의 분석/제안 흐름을 전용 graph의 analyze -> preview 흐름으로 이전한다.

### 작업

```text
- validate_factory_state node 구현
- calculate_metrics node 구현
- detect_bottlenecks node 구현
- build_optimization_candidates node 구현
- validate_preview_candidates node 구현
- return_preview_plan node 구현
```

### 성공 기준

```text
- 출력 적체 예시에서 preview plan이 생성된다.
- preview 응답에는 status, plan_id, factoryRevision, goal, changes, ui_hints가 포함된다.
- 승인 전에는 Unreal command가 생성되지 않는다.
```

### 테스트

```text
- 출력 적체 factory_state -> preview 생성
- 입력 부족 factory_state -> preview 생성
- 빈 factory_state -> validation error
```

## 5. Sprint V2-3: preview 저장과 유효성 관리

### 목표

생성된 preview plan을 저장하고, apply 시점에 유효한 계획인지 검증할 수 있게 한다.

### 작업

```text
- preview plan store 추가
- plan_id 생성
- expires_at 생성
- factoryRevision 저장
- plan 만료 검증
- revision conflict 검증
```

### 성공 기준

```text
- 같은 session_id에서 plan_id로 preview를 조회할 수 있다.
- 만료된 plan은 apply로 넘어가지 않는다.
- factoryRevision이 다르면 revision_conflict를 반환한다.
```

### 테스트

```text
- plan 저장/조회
- plan_expired
- revision_conflict
```

## 6. Sprint V2-4: apply 승인 흐름

### 목표

플레이어 승인 없이 실행이 진행되지 않도록 apply 흐름을 만든다.

### 작업

```text
- validate_apply_request node 구현
- validate_approval node 구현
- validate_selected_changes node 구현
- approval=false 차단
- approved_change_ids 검증
```

### 성공 기준

```text
- approval 없는 apply는 approval_required로 차단된다.
- 존재하지 않는 change_id는 invalid_change_id로 차단된다.
- 승인된 change만 다음 단계로 넘어간다.
```

### 테스트

```text
- approval 없음
- 잘못된 change_id
- 일부 change만 승인
```

## 7. Sprint V2-5: Unreal command와 execution record

### 목표

승인된 변경만 Unreal command payload로 변환하고, 실행 기록을 저장할 준비를 한다.

### 작업

```text
- commands.py 생성
- 허용 명령 enum/schema 정의
- build_unreal_commands node 구현
- execution_record.py 생성
- create_execution_record node 구현
- execution result 수신 schema 정의
```

### 성공 기준

```text
- 허용 명령 목록 밖의 command는 생성되지 않는다.
- command payload는 정해진 schema를 따른다.
- execution record에는 before/after/revision/change_id가 저장된다.
```

### 테스트

```text
- command schema 검증
- 허용되지 않은 command 차단
- execution record 생성
```

## 8. Sprint V2-6: undo 충돌 검증

### 목표

플레이어가 되돌리기를 요청했을 때, 현재 상태와 실행 기록을 비교해 안전한 경우에만 inverse command를 만든다.

### 작업

```text
- undo.py 생성
- load_execution_record node 구현
- compare_current_state_with_recorded_after_state node 구현
- conflict_check node 구현
- build_inverse_commands node 구현
```

### 성공 기준

```text
- 현재 상태가 recorded after와 같으면 inverse command를 만든다.
- 현재 상태가 recorded after와 다르면 undo_conflict를 반환한다.
- conflict가 있으면 Unreal command를 만들지 않는다.
```

### 테스트

```text
- undo 성공
- undo_conflict
- 없는 plan_id undo 요청
```

## 9. Sprint V2-7: 효과 측정과 재분석

### 목표

적용 전후 지표를 비교해 예상 효과와 실제 효과를 구분해 반환한다.

### 작업

```text
- effect_measurement.py 생성
- validate_measurement_window node 구현
- calculate_before_after_metrics node 구현
- compare_expected_and_actual_effects node 구현
- classify_effect_result node 구현
```

### 성공 기준

```text
- 30초와 3 production cycle 조건을 검증한다.
- 예상 효과와 실제 효과를 분리해서 반환한다.
- 악화된 경우 자동 undo가 아니라 reanalyze를 안내한다.
```

### 테스트

```text
- 관찰 시간 부족
- production cycle 부족
- 개선 성공
- 개선 미달
- 악화 후 reanalyze 안내
```

## 10. Sprint V2-8: 통합 smoke test와 문서 최신화

### 목표

v2 흐름을 WebSocket 계약과 문서까지 맞춰 최종 검증한다.

### 작업

```text
- smoke_agent_pipeline process_optimizer v2 시나리오 추가
- agent_test_sample.json 최신화
- unreal_websocket_contract.md 최신화
- process_optimizer_demo_guide.md 최신화
- sprint review 문서 작성
```

### 성공 기준

```text
- analyze -> preview smoke test 통과
- apply 승인 차단/성공 smoke test 통과
- revision conflict smoke test 통과
- undo conflict smoke test 통과
- measure smoke test 통과
```

## 11. 구현 순서 원칙

```text
1. 현재 v1 기능은 유지한다.
2. v2 graph는 처음부터 전체 기능을 만들지 않는다.
3. 먼저 analyze -> preview만 전용 graph로 옮긴다.
4. apply, execution record, undo, measure는 별도 sprint로 붙인다.
5. 각 sprint는 단위 테스트와 smoke test를 함께 갱신한다.
```

## 12. 발표용 설명

```text
process_optimizer v2는 기존 분석/제안 MVP를 전용 LangGraph 구조로 확장하는 단계입니다.
먼저 analyze -> preview 흐름을 graph.py, nodes.py, graph_state.py로 분리하고,
이후 플레이어 승인, 실행 기록, 되돌리기, 효과 측정을 sprint 단위로 추가합니다.
이렇게 나누면 현재 동작하는 v1을 유지하면서도 최종 기획서의 안전한 최적화 Agent 구조로 점진적으로 확장할 수 있습니다.
```
