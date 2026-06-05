# OpenAI SDK LLM Adapter 전환 계획

**Goal:** `openai` provider의 GPT API 호출은 공식 OpenAI Python SDK를 사용하고, `local` provider의 OpenAI-compatible HTTP 호출은 기존 경량 HTTP 경로로 유지한다.

**Scope:** `backend/src/llm/adapter.py`의 OpenAI 호출 방식과 관련 테스트, 의존성, 짧은 문서 설명만 변경한다. 모델명 변경, Responses API 전환, 프롬프트 변경, local provider 재설계는 이번 범위에서 제외한다.

## 배경

현재 `backend/src/llm/adapter.py`는 `openai`와 `local` provider가 같은 `_invoke_openai_compatible()` 함수를 공유한다. 이 함수는 표준 라이브러리 `urllib`로 `/v1/chat/completions`에 직접 요청한다.

이 방식은 local OpenAI-compatible 서버에는 단순하고 유연하지만, 실제 OpenAI GPT API 호출에는 공식 SDK가 더 적합하다. SDK는 typed client, 표준 에러 타입, timeout/base URL 설정, API 변경 대응 측면에서 수동 HTTP보다 유지보수성이 좋다.

## 결정

채택: `openai` provider만 공식 `openai` Python SDK로 전환한다.

- `OpenAILLMAdapter`는 `OpenAI` client의 `chat.completions.create()`를 사용한다.
- `LocalLLMAdapter`는 기존 `_invoke_openai_compatible()` 경로를 유지한다.
- API 응답 계약은 기존처럼 raw text 또는 `None`이다.
- unit test는 실제 OpenAI API를 호출하지 않는다.

## 제외 범위

- Chat Completions API에서 Responses API로 전환하지 않는다.
- `gpt-5.5` 등 테스트 fixture의 모델명을 변경하지 않는다.
- local provider를 SDK 기반으로 통합하지 않는다.
- retry/backoff 정책을 새로 추가하지 않는다.
- 사용자-facing 응답 형식이나 agent pipeline 계약을 변경하지 않는다.

## 구현 대상 파일

- Modify: `backend/pyproject.toml`
  - `openai` dependency 추가
- Modify: `backend/uv.lock`
  - dependency lock 갱신
- Modify: `backend/src/llm/adapter.py`
  - `OpenAILLMAdapter`를 SDK 기반 호출로 변경
  - SDK client 주입용 protocol 또는 factory 추가
  - local OpenAI-compatible HTTP 경로 유지
- Modify: `backend/tests/test_llm_adapter.py`
  - OpenAI adapter 테스트를 SDK client fake 기반으로 변경
  - Local adapter 테스트는 기존 HTTP payload 검증 유지
- Modify: `backend/README.md`
  - OpenAI provider와 local provider의 호출 방식 차이를 짧게 설명

## 구현 체크리스트

- [x] `openai` dependency가 backend runtime dependency에 추가되었다.
- [x] `OpenAILLMAdapter`가 공식 SDK client를 통해 `chat.completions.create()`를 호출한다.
- [x] `OpenAILLMAdapter`는 slot `api_key`가 없으면 provider 호출 없이 `None`을 반환한다.
- [x] `OpenAILLMAdapter`는 slot `model`이 없으면 provider 호출 없이 `None`을 반환한다.
- [x] `OpenAILLMAdapter`는 slot `base_url`이 있으면 SDK client 생성에 반영한다.
- [x] SDK 예외나 비정상 응답은 예외 전파가 아니라 `None` 반환으로 변환한다.
- [x] response text는 `choices[0].message.content`에서 추출한다.
- [x] 빈 문자열 또는 whitespace-only content는 `None`으로 변환한다.
- [x] JSON object 문자열은 기존처럼 trim하지 않고 그대로 반환한다.
- [x] `LocalLLMAdapter`는 기존 OpenAI-compatible HTTP 호출 계약을 유지한다.
- [x] 실제 OpenAI API를 호출하지 않는 unit test로 검증한다.

## Task 1: 의존성 추가

**Files:**

- `backend/pyproject.toml`
- `backend/uv.lock`

Steps:

- [x] `backend/pyproject.toml`의 runtime dependencies에 `openai`를 추가한다.
- [x] `cd backend && uv lock`으로 lock file을 갱신한다.
- [x] `cd backend && uv run python -c "from openai import OpenAI"`로 import smoke를 확인한다.

Acceptance:

- `openai` SDK를 backend runtime code에서 import할 수 있다.
- lock file이 dependency 변경을 반영한다.

## Task 2: OpenAI SDK client 테스트 계약 추가

**Files:**

- `backend/tests/test_llm_adapter.py`

Steps:

- [x] fake SDK client를 추가한다.
- [x] `OpenAILLMAdapter.invoke()`가 `chat.completions.create()`에 `model`, `messages`, `max_tokens`, `temperature`를 전달하는 실패 테스트를 작성한다.
- [x] SDK 응답의 `choices[0].message.content`를 반환하는 실패 테스트를 작성한다.
- [x] api key가 없으면 SDK client를 호출하지 않는 테스트를 유지하거나 갱신한다.
- [x] SDK 예외 발생 시 `None`을 반환하는 테스트를 작성한다.
- [x] empty/whitespace content는 `None`을 반환하는 테스트를 유지한다.
- [x] JSON object raw text를 그대로 반환하는 테스트를 유지한다.
- [x] `LocalLLMAdapter` 테스트는 기존 HTTP fake 기반으로 남겨 local regression을 막는다.

Acceptance:

- OpenAI adapter 테스트는 HTTP URL/payload가 아니라 SDK method call을 검증한다.
- Local adapter 테스트는 계속 HTTP URL/payload를 검증한다.
- 테스트는 실제 network를 사용하지 않는다.

## Task 3: OpenAILLMAdapter SDK 전환

**Files:**

- `backend/src/llm/adapter.py`

Steps:

- [x] `from openai import OpenAI`를 파일 상단에 추가한다.
- [x] OpenAI SDK client에 필요한 protocol을 추가한다.
- [x] `OpenAILLMAdapter`에 테스트 주입용 `client` 또는 `client_factory` 필드를 추가한다.
- [x] 실제 client 생성 시 `api_key`, optional `base_url`, timeout 설정을 반영한다.
- [x] `invoke()`에서 SDK client의 `chat.completions.create()`를 호출한다.
- [x] 응답 content 추출 helper를 SDK response shape에 맞게 추가한다.
- [x] SDK 호출 중 발생하는 예외는 `None`으로 변환한다.

Suggested shape:

```python
completion = client.chat.completions.create(
    model=self.slot.model,
    messages=[{"role": "user", "content": prompt}],
    max_tokens=self.max_output_tokens,
    temperature=self.temperature,
)
```

Acceptance:

- `OpenAILLMAdapter`는 `_invoke_openai_compatible()`를 사용하지 않는다.
- `LocalLLMAdapter`는 `_invoke_openai_compatible()`를 계속 사용한다.
- 기존 `LLMAdapter.invoke(prompt) -> str | None` 계약이 유지된다.

## Task 4: 문서 갱신

**Files:**

- `backend/README.md`

Steps:

- [x] LLM provider 설정 설명에 OpenAI provider는 공식 SDK를 사용한다고 짧게 추가한다.
- [x] local provider는 OpenAI-compatible Chat Completions HTTP endpoint를 사용한다고 명시한다.
- [x] `.env.example`은 기존 설정 키가 유효하면 변경하지 않는다.

Acceptance:

- 사용자가 `openai`와 `local` provider의 호출 경로 차이를 알 수 있다.
- 환경 변수 계약은 불필요하게 바뀌지 않는다.

## 검증 계획

Run:

```bash
cd backend
uv run --extra dev pytest tests/test_llm_adapter.py tests/test_llm_settings.py -q
uv run --extra dev pytest -q
```

Optional smoke:

```bash
cd backend
uv run python -c "from openai import OpenAI; print(OpenAI)"
```

## 위험과 대응

- 위험: SDK client 생성 방식이 테스트 fake와 실제 SDK shape 사이에서 어긋날 수 있다.
  - 대응: protocol은 필요한 최소 surface만 정의하고, import smoke로 검증한다.
- 위험: local provider가 OpenAI SDK 전환에 휘말려 API key 없는 로컬 endpoint 호출이 깨질 수 있다.
  - 대응: Local adapter 테스트를 기존 HTTP payload 검증 그대로 유지한다.
- 위험: SDK dependency 추가로 lock file diff가 커질 수 있다.
  - 대응: dependency 변경 외의 포맷팅/리팩터링은 하지 않는다.
- 위험: 현재 provider 실패 정책은 예외를 삼키고 `None`을 반환한다.
  - 대응: 이번 변경에서는 기존 정책을 유지하고, logging 개선은 별도 작업으로 남긴다.

## 완료 기준

- OpenAI provider는 공식 SDK를 통해 GPT API를 호출한다.
- Local provider는 기존 OpenAI-compatible HTTP 호출을 유지한다.
- Adapter/settings 관련 테스트와 전체 backend 테스트가 통과한다.
- README가 변경된 호출 방식을 짧게 설명한다.
