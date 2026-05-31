# Task RED - Rename manual_qa to operator_guide

## Scope

`manual_qa` top-level/domain Agent id와 package name을 `operator_guide`로 rename한다.

## Naming decision

- `factory_support`는 다른 Agent도 모두 support 역할을 하므로 구분력이 약하다.
- `operator_guide`는 사용자가 공장/설비/레시피를 이해하고 조작하도록 안내하는 도메인을 드러낸다.

## RED verification

Production code 수정 전에 테스트 기대를 먼저 `operator_guide`로 바꾼다.

예상 실패:

- `create_default_agent_router()`는 아직 `manual_qa.*` leaf Agent id를 반환한다.
- top-level routing allowlist는 아직 `manual_qa`를 포함한다.
- pipeline은 아직 `operator_guide` 경로와 `operator_guide.*` leaf Agent id를 모른다.

실제 RED 결과:

- 실행: `uv run --extra dev pytest tests/test_agent_contracts.py tests/test_message_router.py::test_pipeline_uses_prompt_based_operator_guide_sub_agent_routing tests/test_scenario_harness.py::test_explicit_sub_agent_scenarios tests/test_agent_leaf_behaviors.py::test_operator_guide_leaf_agents_return_normalized_fallbacks -q`
- 결과: collection error.
- 실패 이유: 테스트가 `agents.operator_guide.*` import를 기대하지만 production package가 아직 `agents/manual_qa/`라 `ModuleNotFoundError: No module named 'agents.operator_guide'`가 발생했다.

## Acceptance

- top-level Agent id는 `operator_guide`다.
- leaf Agent id는 `operator_guide.recipe_explainer`, `operator_guide.machine_help`, `operator_guide.troubleshooter`다.
- package path는 `agents/operator_guide/`다.
- Domain Orchestrator class는 `OperatorGuideAgent`다.
- active backend code/tests/docs에 `manual_qa` id/path/class 기준이 남지 않는다.
- 리뷰 결과는 날짜별 파일 `_workspace/factory-agent/review-feedback-2026-05-31.md`에 남긴다.

## GREEN verification

- 실행: `uv run --extra dev pytest tests/test_agent_contracts.py tests/test_message_router.py::test_pipeline_uses_prompt_based_operator_guide_sub_agent_routing tests/test_scenario_harness.py::test_explicit_sub_agent_scenarios tests/test_agent_leaf_behaviors.py::test_operator_guide_leaf_agents_return_normalized_fallbacks -q`
- 결과: 13 passed.
