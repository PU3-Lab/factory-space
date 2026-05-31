# Task 4.3 RED - OpenAI-compatible adapter

## Scope

Sprint 4.3은 `OpenAiLlmAdapter`가 OpenAI Chat Completions compatible HTTP endpoint를 1회 호출하고, raw assistant content를 반환하는 계약을 고정한다.

## RED verification

Production code 수정 전에 다음 실패 테스트를 추가한다.

- `test_openai_llm_adapter_returns_response_text`
- `test_openai_llm_adapter_returns_none_without_api_key`
- `test_openai_llm_adapter_returns_none_for_provider_error`
- `test_openai_llm_adapter_preserves_json_object_response_text`

예상 실패:

- 현재 `OpenAiLlmAdapter`는 placeholder라 `http_client`, `timeout_ms`, `max_output_tokens`, `temperature` 주입을 받지 못하거나 항상 `None`을 반환한다.

## Acceptance captured by tests

- endpoint는 OpenAI-compatible 기본값 `https://api.openai.com/v1/chat/completions`를 사용한다.
- Authorization header는 slot API key를 Bearer token으로 전달한다.
- request body의 model은 slot 설정값에서만 온다.
- prompt는 user role message로 전달한다.
- provider 예외는 전파하지 않고 `None`으로 변환한다.
- assistant message content가 JSON object 문자열이어도 trimming/parsing 없이 그대로 반환한다.
