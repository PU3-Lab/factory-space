# Task RED - Sub-agent structured prompt routing

## Scope

`manual_qa`와 `quest_generator` 도메인 오케스트레이터의 sub-agent routing도 top-level routing과 같은 structured prompt 계약으로 맞춘다.

## RED verification

Production code 수정 전에 다음 실패 테스트를 먼저 만든다.

- `test_pipeline_uses_prompt_based_top_level_routing`
- `test_pipeline_uses_prompt_based_manual_sub_agent_routing`
- `test_sub_orchestrators_build_structured_routing_prompts`
- `test_pipeline_rejects_json_sub_agent_routing_output`

예상 실패:

- 현재 sub-agent routing prompt는 compact JSON을 요구한다.
- 현재 sub-agent routing parser는 JSON을 decode한 뒤 `candidate in *_SUB_AGENT_IDS`로 검증한다.
- plain sub-agent id 문자열은 거부되고, JSON routing output은 허용된다.

실제 RED 결과:

- 실행: `uv run --extra dev pytest tests/test_message_router.py::test_pipeline_uses_prompt_based_top_level_routing tests/test_message_router.py::test_pipeline_uses_prompt_based_manual_sub_agent_routing tests/test_message_router.py::test_pipeline_rejects_json_sub_agent_routing_output tests/test_agent_contracts.py -q`
- 결과: 4 failed, 1 passed.
- 실패 이유: plain sub-agent id 문자열은 `ROUTING_UNAVAILABLE`로 실패했고, JSON sub-agent routing output은 기존 parser가 허용했으며, sub-agent prompt에는 structured prompt 섹션이 없었다.

## Acceptance captured by tests

- domain leaf routing prompt는 `[ROLE]`, `[TASK]`, `[ALLOWED_LEAF_AGENT_IDS]`, `[REQUEST_CONTEXT]`, `[REQUEST_PAYLOAD]`, `[OUTPUT_CONTRACT]` 섹션을 가진다.
- sub-agent routing output은 허용된 sub-agent id 문자열 하나만 허용한다.
- JSON, markdown, 설명, reason, 따옴표, 코드블록은 sub-agent routing output으로 허용하지 않는다.
- route node는 raw model output을 strip해서 `selectedLeafAgent` state에 기록한다.
- sub-agent 유효성 검증은 LangGraph conditional edge에서 한다.
