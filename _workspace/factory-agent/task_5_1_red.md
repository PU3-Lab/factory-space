# Task 5.1 RED - LangGraph LLM fallback wiring

## Scope

Sprint 5.1은 LangGraph pipeline generation 단계에 `default -> fallback1 -> fallback2 -> deterministic fallback` LLM slot 경로를 연결한다.

## RED verification

Production code 수정 전에 다음 실패 테스트를 추가한다.

- `test_pipeline_default_settings_without_api_returns_deterministic_fallback`
- `test_pipeline_uses_settings_slot_adapters_before_deterministic_fallback`

예상 실패:

- 현재 `AgentPipeline`은 `llm_settings`와 adapter factory를 받지 않는다.
- 현재 graph는 단일 `llm` adapter만 호출하고 settings의 `default/fallback1/fallback2` slot을 만들지 않는다.

## Acceptance captured by tests

- 명시 `llm` 주입을 쓰지 않는 `AgentPipeline`은 settings의 세 slot adapter를 생성한다.
- 세 slot이 모두 `none`이면 외부 API 없이 deterministic fallback response를 반환한다.
- default slot이 실패하고 fallback1이 성공하면 deterministic fallback 대신 LLM response를 사용한다.
- 성공한 LLM slot/provider/model은 response metadata에 남는다.
- fallback 순서는 adapter 내부가 아니라 LangGraph pipeline 경로에서 검증한다.
