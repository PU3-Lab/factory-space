# operator_guide OpenAI Responses API 전환 계획

## 문제

기본 모델 `gpt-5.4-nano`는 현재 OpenAI 프로젝트의 모델 목록에 존재하지만, 기존 `OpenAILLMAdapter`가 사용하는 Chat Completions API 호출에서는 `404 model not found`가 발생한다.

같은 모델을 Responses API로 호출하면 정상 응답한다.

## 목표

공통 `LLMAdapter` 인터페이스와 operator_guide pipeline은 유지하면서 OpenAI provider 구현만 Responses API로 전환한다.

## 수정 범위

1. `OpenAILLMAdapter`가 `client.responses.create()`를 호출한다.
2. 기존 message 목록을 Responses API의 `input`으로 전달한다.
3. `max_output_tokens`와 `temperature` 설정을 유지한다.
4. 응답의 `output_text`를 기존과 같은 원시 문자열로 반환한다.
5. Google 및 local adapter는 변경하지 않는다.

## 검증

1. OpenAI adapter 단위 테스트
2. LLM fallback 및 operator_guide 관련 테스트
3. Ruff 검사
4. `gpt-5.4-nano` 실제 최소 smoke test

## 작업 로그

- 2026-06-22: 모델 목록에서 `gpt-5.4-nano` 접근 가능 여부를 확인했다.
- 2026-06-22: Responses API에서 messages, temperature, max_output_tokens를 사용한 실제 호출이 성공함을 확인했다.

