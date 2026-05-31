# Task 4.4 RED - Local OpenAI-compatible adapter

## Scope

Sprint 4.4는 `LocalLLMAdapter`가 local OpenAI-compatible endpoint를 1회 호출하고, raw assistant content를 반환하는 계약을 고정한다.

## RED verification

Production code 수정 전에 다음 실패 테스트를 추가한다.

- `test_local_llm_adapter_returns_response_text_without_api_key`
- `test_local_llm_adapter_returns_none_without_base_url`
- `test_local_llm_adapter_returns_none_for_endpoint_error`

예상 실패:

- 현재 `LocalLLMAdapter`는 placeholder라 `http_client`, `timeout_ms`, `max_output_tokens`, `temperature` 주입을 받지 못하거나 항상 `None`을 반환한다.

## Acceptance captured by tests

- local endpoint는 slot `base_url`만 사용한다.
- local provider는 API key 없이도 요청을 보낸다.
- request body의 model은 slot 설정값에서만 온다.
- prompt는 user role message로 전달한다.
- endpoint 실패는 예외 전파가 아니라 `None`으로 변환한다.
