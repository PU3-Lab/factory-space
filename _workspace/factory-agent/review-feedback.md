# Review Feedback

이 파일은 코드 리뷰에서 나온 문제와 후속 수정 대상을 별도로 추적한다.

## 2026-05-31 Pipeline Review

Status: resolved

Resolved by:

- `b3c7f1e fix: cache hit metadata 보존`
- `7d09463 fix: validation error correlation 보존`

### 1. Malformed envelope request correlation

- severity: high
- file: `backend/src/agents/pipeline/runtime.py`
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
- file: `backend/src/agents/pipeline/runtime.py`
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

## 2026-05-31 Google Gen AI Adapter Re-review

Status: resolved

Reviewer: `Dalton` sub-agent

### 1. Client initialization failure 테스트 누락

- severity: low
- file: `backend/tests/test_llm_adapter.py`

문제:

- `invoke()` 구현은 `_create_google_client()` 예외를 `None`으로 흡수하도록 수정됐지만, 이 경로를 고정하는 테스트가 없었다.
- client 생성이 다시 `try` 밖으로 이동해도 기존 테스트만으로는 회귀를 잡기 어렵다.

필요 작업:

- `_create_google_client`를 monkeypatch로 예외 발생시키고 `invoke()`가 `None`을 반환하는 테스트를 추가한다.

수정:

- `test_google_llm_adapter_returns_none_when_client_creation_fails`를 추가했다.

최종 재리뷰:

- reviewer: `Averroes` sub-agent
- result: no unresolved findings
- 확인: client initialization failure 테스트가 `_create_google_client` 예외 경로를 고정하고, `review-feedback.md` 상태값과 `compound.md` 기록이 현재 변경 이력과 일치한다.

## 2026-05-31 OpenAI-compatible Adapter Review

Status: resolved

Reviewer: `Aristotle` sub-agent

### 1. HTTP non-2xx 응답 테스트 누락

- severity: low
- file: `backend/tests/test_llm_adapter.py`

문제:

- provider 실패 요구사항이 예외 케이스로만 고정되어 있다.
- 구현은 non-2xx 응답을 `None`으로 처리하지만, 429/500 같은 HTTP error response 경로를 고정하는 테스트가 없다.

영향:

- 이후 변경에서 non-2xx 응답 본문을 정상 응답처럼 파싱하거나 예외 전파로 회귀해도 테스트가 놓칠 수 있다.

필요 작업:

- fake HTTP client가 `FakeHttpResponse(500, {...})`를 반환할 때 `invoke()`가 `None`을 반환하는 테스트를 추가한다.

수정:

- `test_openai_llm_adapter_returns_none_for_http_error_response`를 추가했다.

### 2. 빈 assistant content 테스트 누락

- severity: low
- file: `backend/tests/test_llm_adapter.py`

문제:

- raw 문자열 보존 테스트는 있지만 OpenAI adapter가 빈 assistant content를 `None`으로 반환하는 테스트가 없다.

영향:

- 빈 응답 판단이 제거되거나 `strip()` 반환으로 바뀌는 회귀를 놓칠 수 있다.

필요 작업:

- `choices[0].message.content`가 `""` 또는 blank-only 문자열일 때 `None`을 반환하는 테스트를 추가한다.

수정:

- `test_openai_llm_adapter_returns_none_for_empty_response_text`를 추가했다.

최종 재리뷰:

- reviewer: `Erdos` sub-agent
- result: no unresolved findings
- 확인: HTTP non-2xx 응답과 blank-only assistant content 테스트가 추가되었고, Sprint 4.3 체크 상태가 구현/테스트 완료 상태와 일치한다.

## 2026-05-31 Local OpenAI-compatible Adapter Review

Status: resolved

Reviewer: `Bacon` sub-agent

최종 리뷰:

- result: no unresolved findings
- 확인: `LocalLLMAdapter`가 slot의 `base_url`과 `model`만 사용해 Chat Completions compatible endpoint를 호출한다.
- 확인: local provider는 API key 없이 `Authorization` header 없이 요청한다.
- 확인: provider/endpoint 실패는 예외 전파 없이 `None`으로 수렴한다.
- 확인: fallback 순서는 adapter에 들어가 있지 않다.
- 확인: 함수/메서드 내부 import 금지 규칙이 문서에 반영되었고 내부 import 검색 결과가 비어 있다.

## 2026-05-31 LLM Acronym Rename Review

Status: resolved

Reviewer: `Boyle` sub-agent

최종 리뷰:

- result: no unresolved findings
- 확인: 기존 mixed-case LLM 타입/클래스 표기를 `LLM` acronym 표기로 정리했다.
- 확인: package/module path `llm`, snake_case 함수/변수, `FACTORY_LLM_*` env var는 유지했다.
- 확인: 내부 import 검색, Ruff, 전체 pytest 검증이 통과했다.

## 2026-05-31 LangGraph LLM Fallback Wiring Review

Status: resolved

Reviewer: `Jason` sub-agent

### 1. fallback2 및 전체 실패 순서 테스트 누락

- severity: low
- file: `backend/tests/test_pipeline_edges.py`

문제:

- 현재 테스트는 `default -> fallback1 성공 -> fallback2 미호출`만 직접 검증한다.
- 구현에는 `fallback2` edge와 deterministic fallback edge가 있지만, `default/fallback1 실패 후 fallback2 성공` 및 `default/fallback1/fallback2 모두 실패 후 deterministic fallback` 순서가 테스트로 고정되어 있지 않다.
- `_workspace/factory-agent/task_5_1_red.md`가 "fallback 순서는 LangGraph pipeline 경로에서 검증"한다고 적고 있어 테스트 주장과 실제 커버리지 사이에 간극이 있다.

영향:

- fallback2 edge 또는 세 slot 실패 후 deterministic fallback edge가 회귀해도 테스트가 놓칠 수 있다.

필요 작업:

- default/fallback1 실패 후 fallback2 성공 경로 테스트를 추가한다.
- default/fallback1/fallback2 모두 실패 후 deterministic fallback 경로 테스트를 추가한다.

수정:

- `test_pipeline_uses_fallback2_when_default_and_fallback1_fail`를 추가했다.
- `test_pipeline_uses_deterministic_fallback_after_all_slots_fail`를 추가했다.

구조 정리:

- `backend/src/agents/pipeline.py`를 `backend/src/agents/pipeline/` 패키지로 이동했다.
- `runtime.py`, `graph_edges.py`, `llm_fallback.py`, `state.py`, `utils.py`로 기능별 책임을 분리했다.
- source 파일 500줄 제한 확인: `backend/src/agents/pipeline/runtime.py` 430줄, 나머지 pipeline package 파일은 129줄 이하.

검증:

- `uv run --extra dev ruff check .` 통과
- `uv run --extra dev pytest tests/test_pipeline_edges.py -q` 통과: 15 passed
- `uv run --extra dev pytest -q` 통과: 86 passed
- 함수/메서드 내부 import 검색 결과 없음

재리뷰:

- reviewer: `Singer` sub-agent
- status: no unresolved findings after fix

### 2. LangGraph 문서의 cache hit 경로가 실제 edge와 다름

- severity: low
- file: `backend/src/FOLDER_ROLES.md`

문제:

- 문서 다이어그램은 cache hit 경로를 `build_cached_response -> END`로 표시했다.
- 실제 graph는 `build_cached_response -> build_agent_response -> END`로 최종 envelope 생성 단계를 경유한다.

영향:

- 문서만 보고 cache hit 응답이 envelope 생성 단계를 우회한다고 오해할 수 있다.

수정:

- Mermaid diagram의 cache hit 경로를 `BuildCachedResponse --> BuildResponse --> End`로 수정했다.

최종 재리뷰:

- result: no unresolved findings
- 확인: `FOLDER_ROLES.md`의 cache hit 경로가 `graph_edges.py`의 실제 edge와 일치한다.
- 확인: `uv run --extra dev pytest tests/test_pipeline_edges.py -q` 통과: 15 passed

후속 구조 반영:

- 질문: `build_agent_graph`를 `AgentPipeline` 안에 넣으면 인자로 받을 필요가 없는가?
- 답변: 맞다. `AgentPipeline`이 `router`, `cache`, `llm`, `llm_settings`, `llm_adapter_factory`를 이미 소유하므로 graph 생성은 내부 `_build_graph()`가 `self` 의존성을 사용한다.
- 수정: public `build_agent_graph` export를 제거하고, compiled graph 확인 테스트를 `AgentPipeline(...).graph` 기준으로 변경했다.
- 검증: `uv run --extra dev pytest tests/test_pipeline_edges.py -q` 통과: 15 passed
- 검증: `uv run --extra dev ruff check .` 통과
- 검증: `uv run --extra dev pytest -q` 통과: 86 passed
- status: no unresolved findings after `_build_graph()` refactor

최종 재리뷰:

- reviewer: `Singer` sub-agent
- result: no unresolved findings
- 확인: `build_agent_graph` 제거 후 repo 내부 사용처는 결정/리뷰 기록 문서뿐이다.
- 확인: `_build_graph()`가 `AgentPipeline` 내부에서 `self` 의존성을 사용하는 구조가 중복 DI 경로보다 단순하다.
- 확인: `llm` 명시 주입과 `default -> fallback1 -> fallback2 -> deterministic fallback` 테스트 계약이 유지된다.
