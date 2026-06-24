# Process Optimizer v2 Sprint 2 계획

## 목표

현재 v1에 있는 `analyze -> suggestion` 흐름을 v2 전용 LangGraph의 `analyze -> preview` 흐름으로 이전한다.

이 sprint에서는 아직 실행 명령을 만들지 않는다. 플레이어에게 보여 줄 미리보기 계획만 생성한다.

## 구현 범위

```text
- factory_state 검증
- 분석 지표 계산
- 병목 탐지
- preview 후보 생성
- preview 후보 검증
- preview 응답 schema 구성
```

## 수정 파일

```text
backend/src/agents/process_optimizer/graph_state.py
backend/src/agents/process_optimizer/nodes.py
backend/src/agents/process_optimizer/graph.py
backend/src/agents/process_optimizer/analyzer.py
backend/src/agents/process_optimizer/suggestion.py
backend/src/agents/process_optimizer/schemas.py
backend/tests/test_process_optimizer_graph.py
```

## Graph 흐름

```text
START
-> validate_factory_state
-> calculate_metrics
-> detect_bottlenecks
-> build_optimization_candidates
-> validate_preview_candidates
-> return_preview_plan
-> END
```

## Preview 응답 필드

```text
status: "preview"
plan_id
factoryRevision
goal
summary
changes
expected_effect
ui_hints
```

## 성공 기준

```text
- 출력 적체 상태에서 preview plan이 생성된다.
- 입력 부족 상태에서 preview plan이 생성된다.
- 빈 factory_state는 validation error를 반환한다.
- preview 단계에서는 Unreal command를 생성하지 않는다.
- changes는 최대 3개를 넘지 않는다.
```

## 테스트 계획

```text
- 출력 적체 factory_state -> preview 생성
- 입력 부족 factory_state -> preview 생성
- 컨베이어 혼잡 factory_state -> highlight target 포함
- 빈 factory_state -> error
- changes 최대 3개 제한
```

## 완료 후 확인 질문

```text
- v1 analyzer/suggestion 로직을 재사용했는가?
- graph state에 분석 결과가 명확히 남는가?
- preview와 실행 command가 분리되어 있는가?
```

