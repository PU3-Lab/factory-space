# Process Optimizer v2 Sprint 2 코드리뷰

`process_optimizer` 전용 LangGraph의 `analyze -> preview` 이전 구현을 기획 문서 기준으로 점검했습니다.

## 1. 구현 개요

- 목적: v1의 분석/제안 로직을 v2 전용 LangGraph의 preview 흐름으로 이전했습니다.
- 적용 노드 흐름: `START -> validate_factory_state -> calculate_metrics -> detect_bottlenecks -> build_optimization_candidates -> validate_preview_candidates -> return_preview_plan -> END`
- 기존 v1 운영 라우팅은 변경하지 않았습니다.

## 2. 파일별 리뷰

### 2.1 `backend/src/agents/process_optimizer/graph_state.py`

- `metrics`, `bottlenecks`, `suggestions`, `ui_hints` 필드를 추가해 graph 노드 사이의 분석 결과 흐름을 명시했습니다.
- Sprint 2 범위에서 필요한 preview 생성 상태를 충분히 담습니다.

### 2.2 `backend/src/agents/process_optimizer/nodes.py`

- `validate_factory_state`는 factory snapshot 누락과 빈 공장 상태를 error preview로 분리합니다.
- `calculate_metrics`는 기존 `FactoryStateAnalyzerTool`을 재사용합니다.
- `detect_bottlenecks`는 분석 보고서에서 병목 요약을 state에 기록합니다.
- `build_optimization_candidates`는 기존 `OptimizationSuggestionTool`을 재사용해 최대 3개 preview 후보와 UI 힌트를 생성합니다.
- `validate_preview_candidates`는 기존 `SuggestionValidationTool`로 명령 주입과 최대 개수 규칙을 검증합니다.
- `return_preview_plan`은 `status: "preview"` 응답을 조립하되, 정량 개선율을 임의로 만들지 않고 `estimated: false`와 해결 대상 count만 반환합니다.

### 2.3 `backend/src/agents/process_optimizer/graph.py`

- Sprint 2 기획 흐름대로 6개 노드를 순차 연결했습니다.
- 아직 apply, undo, measure는 연결하지 않아 preview 단계와 실행 단계를 분리했습니다.

### 2.4 `backend/src/agents/process_optimizer/schemas.py`

- `ProcessOptimizerResponse.status`에 `preview`를 추가했습니다.
- preview 응답에 필요한 `plan_id`, `changes`, `expected_effect` 필드를 추가했습니다.

### 2.5 `backend/tests/test_process_optimizer_graph.py`

- graph import/compile을 검증합니다.
- factory state 누락과 빈 상태 error를 검증합니다.
- 입력 부족, 출력 적체, 컨베이어 혼잡 preview를 검증합니다.
- changes가 최대 3개로 제한되는지 검증합니다.
- 임의 개선율 필드가 생성되지 않는지 검증합니다.

## 3. 테스트 검증 결과

```text
uv run --env-file .env.prod pytest tests/test_process_optimizer_graph.py tests/test_process_optimizer.py tests/test_process_optimizer_prompt.py -q

20 passed
```

## 4. 결론

Sprint 2 기획 범위는 충족했습니다. 전용 LangGraph가 실제 분석 지표를 계산하고 preview 제안을 생성하며, 아직 실행 명령을 만들지 않는 구조가 유지됩니다. 다음 단계는 Sprint 3에서 preview plan 저장, 만료, revision 충돌 검증을 붙이는 것입니다.
