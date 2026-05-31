# Task RED - LLM decision routing edge coverage

## Scope

Sprint 6.1은 `test_pipeline_edges.py`에서 LLM routing decision 경로를 명시적으로 고정한다.

## RED verification

이번 작업은 이전 Sprint에서 이미 구현된 routing 계약을 계획서의 지정 테스트 파일에 보강하는 범위다. Production code는 수정하지 않는다.

추가할 테스트:

- top-level orchestrator가 raw 문자열 `operator_guide`를 반환하면 `operator_guide` 경로로 분기한다.
- `quest_generator` 도메인 오케스트레이터가 raw 문자열 `quest_generator.production_quest`를 반환하면 해당 leaf Agent로 분기한다.
- top-level routing JSON output은 `ROUTING_UNAVAILABLE`로 종료된다.
- sub-agent routing JSON output은 `ROUTING_UNAVAILABLE`로 종료된다.

RED 기대:

- 현재 구현은 이미 이 계약을 만족할 가능성이 높다.
- 새 테스트가 바로 통과하면 이번 변경은 구현 RED가 아니라 Sprint 6.1 계획 추적용 regression coverage로 기록한다.

## Acceptance captured by tests

- routing output은 JSON이 아니라 허용 id 문자열 하나다.
- invalid decision은 임의 fallback agent를 고르지 않는다.
- top-level Agent와 leaf Agent 검증은 LangGraph conditional edge가 담당한다.
