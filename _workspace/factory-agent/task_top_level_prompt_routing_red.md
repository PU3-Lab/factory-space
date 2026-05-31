# Task RED - Top-level agent prompt routing

## Scope

최상위 Agent 선택은 명시 `agent` 값이 있어도 코드 로직으로 확정하지 않고, orchestrator prompt의 모델 응답으로만 확정한다.

## RED verification

Production code 수정 전에 다음 실패 테스트를 먼저 만든다.

- `test_pipeline_routes_explicit_agent_through_top_level_prompt`

RED 확인:

- `uv run --extra dev pytest tests/test_message_router.py::test_pipeline_routes_explicit_agent_through_top_level_prompt -q`
- 결과: 실패
- 실패 이유: 명시 `agent`가 있으면 첫 prompt가 서버 전체 오케스트레이터 prompt가 아니라 leaf Agent prompt였고, prompt 수가 1개였다.

예상 실패:

- 기존 `route_top_agent`는 `envelope.agent`가 있으면 `OrchestratorAgent.build_routing_prompt()`를 호출하지 않았다.
- 따라서 명시 `agent` 요청에서 첫 LLM prompt는 서버 전체 오케스트레이터 prompt가 아니라 leaf Agent prompt다.

## Acceptance captured by tests

- 명시 `agent`는 prompt의 hint로만 사용한다.
- 최상위 Agent decision은 항상 orchestrator prompt의 raw 문자열을 strip한 값으로 `selectedAgent` state에 기록한다.
- 실제 Agent 분기는 `route_selected_agent` LangGraph conditional edge가 담당한다.
- 모델 routing 결과가 없으면 명시 `agent`가 있어도 `ROUTING_UNAVAILABLE`로 끝난다.
- 서브 에이전트 선택은 도메인 오케스트레이터의 structured prompt가 담당하고, LangGraph conditional edge가 허용 id를 검증한다.
