# Review Feedback

이 파일은 코드 리뷰에서 나온 문제와 후속 수정 대상을 별도로 추적한다.

## 2026-05-31 Pipeline Review

Status: resolved

Resolved by:

- `b3c7f1e fix: cache hit metadata 보존`
- `7d09463 fix: validation error correlation 보존`

### 1. Malformed envelope request correlation

- severity: high
- file: `backend/src/agents/pipeline.py`
- related decision: `backend/src/DECISION_LOG.md` section 15.1

문제:

- `AgentPipeline.run()`이 `AgentRequestEnvelope.model_validate()`를 먼저 호출한다.
- 잘못된 `type` 또는 object가 아닌 `payload`는 graph 내부 validation node에 도달하기 전에 `INVALID_ENVELOPE`로 끝난다.
- 이 경로에서는 raw message의 `request_id`, `session_id`, `client_id`, `agent`가 error envelope에 보존되지 않는다.

영향:

- 클라이언트가 어떤 요청의 오류인지 매칭하기 어렵다.
- `INVALID_MESSAGE_TYPE`, `INVALID_PAYLOAD` 분기가 dict 입력에서는 사실상 unreachable 상태다.

필요 작업:

- 잘못된 type과 invalid payload에서도 correlation field가 보존되는 실패 테스트를 먼저 추가한다.
- validation error builder 또는 envelope parsing flow를 수정한다.
- WebSocket 경유 오류 응답에서도 같은 보존 규칙이 적용되는지 확인한다.

### 2. Cache hit metadata loss

- severity: medium
- file: `backend/src/agents/pipeline.py`
- related decision: `backend/src/DECISION_LOG.md` section 15.2

문제:

- cache write는 `responsePayload`만 저장한다.
- cache hit 응답은 metadata를 `{"cache": "hit"}`로 새로 만든다.
- 첫 응답에 있던 `fallback: true`, `llm: used` 같은 metadata가 반복 요청에서 사라진다.

영향:

- 같은 요청의 첫 응답과 cache hit 응답이 metadata 기준으로 달라진다.
- fallback/LLM 사용 여부 추적이 불안정해진다.

필요 작업:

- cache entry에 payload와 metadata를 함께 저장한다.
- cache hit 응답은 원래 metadata를 유지하고 `cache: hit`만 추가한다.
- 첫 응답과 cache hit 응답의 metadata 일관성을 검증하는 실패 테스트를 먼저 추가한다.

## 2026-05-31 LLM Settings / Dependency Review

Status: resolved

Reviewer: `Socrates` sub-agent

### 1. `from_env({})`가 실제 환경 변수를 읽는 문제

- severity: medium
- file: `backend/src/llm/settings.py`
- status: fixed before commit

문제:

- 직전 커밋 `a5aa46a` 기준 `source = env or os.environ` 때문에 빈 mapping을 명시해도 실제 `os.environ`을 읽었다.
- `FACTORY_LLM_DEFAULT_PROVIDER` 같은 ambient env가 있으면 env 미설정/CI 기본값 `none` 계약이 깨질 수 있었다.

수정:

- `env is None`일 때만 `os.environ`을 사용하도록 변경했다.
- `tests/test_llm_settings.py`에서 `monkeypatch`로 실제 환경 변수가 있어도 `from_env({})`는 `none`을 반환하도록 검증한다.

### 2. 의존성 정리 리뷰

- severity: none
- status: no unresolved findings

확인:

- `google-generativeai`, `langchain`, `langchain-google-genai` 제거.
- `google-genai>=1.33.0` 추가.
- FastAPI는 최신 0.136 patch 라인인 `fastapi>=0.136.3,<0.137.0`로 축소.
- `uv.lock` root metadata와 package lock이 `pyproject.toml`과 일치한다.

## 2026-05-31 Google Gen AI Adapter Review

Status: resolved

Reviewer: `Harvey` sub-agent

### 1. API key/client initialization 예외가 `invoke()` 밖으로 전파될 수 있음

- severity: medium
- file: `backend/src/llm/adapter.py`

문제:

- `_create_google_client()` 호출이 `try` 밖에 있다.
- `api_key=""` 또는 SDK client 초기화 예외가 발생하면 `invoke()`가 `None`을 반환하지 않고 예외를 전파할 수 있다.

필요 작업:

- missing/blank API key와 client initialization failure가 `None`을 반환하는 테스트를 추가한다.
- client 생성도 `try` 경계 안에서 처리하거나 `_create_google_client()`가 예외를 삼키도록 수정한다.

수정:

- blank API key 테스트를 추가했다.
- `_create_google_client()`는 falsy key를 `None`으로 처리한다.
- client 생성과 provider 호출을 같은 `try` 경계 안에서 처리한다.

### 2. 성공 응답이 raw text로 반환되지 않음

- severity: low
- file: `backend/src/llm/adapter.py`

문제:

- 현재 구현은 `response.text.strip()`을 반환한다.
- 계약이 raw text 반환이라면 whitespace 보존이 필요하고, `strip()`은 빈 응답 판단에만 써야 한다.

필요 작업:

- whitespace가 포함된 응답을 그대로 반환하는 테스트를 추가한다.
- 빈 응답 확인은 `text.strip()`으로 하되 반환값은 원본 `text`로 유지한다.

수정:

- whitespace 포함 응답을 raw text 그대로 반환하도록 테스트와 구현을 수정했다.

### 3. 필수 edge case 테스트 누락

- severity: low
- file: `backend/tests/test_llm_adapter.py`

문제:

- non-string `response.text`와 missing API key 테스트가 없다.

필요 작업:

- non-string `response.text`는 `None` 반환으로 고정한다.
- missing/blank API key는 실제 Google API 호출 없이 `None` 반환으로 고정한다.

수정:

- non-string response text 테스트를 추가했다.
- missing/blank API key 테스트를 추가했다.
