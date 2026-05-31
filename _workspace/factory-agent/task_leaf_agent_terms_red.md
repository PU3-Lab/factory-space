# Task RED - Leaf Agent terminology

## Scope

pipeline state와 response metadata에서 실행 대상 Agent를 `selectedSubAgent`로 부르던 것을 실제 의미에 맞게 `selectedLeafAgent`로 바꾼다.

## RED verification

Production code 수정 전에 다음 기대를 테스트에 반영한다.

- `agent.response.payload.metadata.selectedLeafAgent`가 실행 leaf Agent id를 가진다.
- `selectedSubAgent` metadata는 더 이상 public response에 노출하지 않는다.
- LangGraph edge predicate 이름은 `route_selected_leaf_agent`처럼 leaf Agent 검증 역할을 드러낸다.

예상 실패:

- 현재 pipeline state와 response metadata는 `selectedSubAgent`를 사용한다.
- 공통 edge 이름도 `route_sub_agent_result`라서 leaf top-level Agent까지 sub-agent처럼 보이게 한다.

실제 RED 결과:

- 실행: `uv run --extra dev pytest tests/test_message_router.py::test_pipeline_uses_prompt_based_top_level_routing tests/test_scenario_harness.py -q`
- 결과: 7 failed, 1 passed.
- 실패 이유: response metadata에 `selectedLeafAgent`가 없고 기존 `selectedSubAgent`만 노출됐다.

GREEN 결과:

- 실행: `uv run --extra dev pytest tests/test_message_router.py::test_pipeline_uses_prompt_based_top_level_routing tests/test_scenario_harness.py tests/test_agent_contracts.py -q`
- 결과: 10 passed.

## Terminology

- `selectedAgent`: Global Orchestrator가 고른 top-level Agent id.
- `selectedLeafAgent`: 실제 prompt/generation/fallback 실행에 사용할 leaf Agent id.
- `sub_agent`: public request payload에서 도메인 내부 leaf Agent를 명시하고 싶을 때 쓰는 입력 힌트.
- `route_selected_leaf_agent`: 선택된 leaf Agent id가 top-level Agent의 허용 leaf 목록에 속하는지 검증하는 LangGraph conditional edge predicate.
