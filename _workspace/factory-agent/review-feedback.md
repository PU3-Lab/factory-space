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

## 2026-05-31 Top-level Prompt Routing Review

Status: resolved

질문:

- 최상위 Agent를 왜 코드 로직으로 찾는가?
- LangGraph conditional node/edge로 구분해야 하지 않는가?

수정:

- `route_top_agent`는 명시 `agent`가 있어도 직접 선택하지 않고, 항상 orchestrator prompt를 호출한다.
- 명시 `agent`는 `OrchestratorAgent.build_routing_prompt(..., requested_agent=...)`의 hint로만 전달한다.
- 최상위 Agent decision은 모델이 반환한 문자열을 `selectedAgent` state에 기록한다.
- 실제 Agent 경로 분기는 기존 `route_selected_agent` LangGraph conditional edge가 담당한다.

계약 변경:

- routing model이 유효한 최상위 Agent 결정을 반환하지 못하면 명시 `agent`가 있어도 `ROUTING_UNAVAILABLE`로 종료한다.
- API key나 local routing model이 없는 기본 WebSocket `agent.request`는 deterministic fallback 전에 routing 단계에서 실패할 수 있다.

검증:

- RED: `uv run --extra dev pytest tests/test_message_router.py::test_pipeline_routes_explicit_agent_through_top_level_prompt -q` 실패 확인
- GREEN: `uv run --extra dev pytest tests/test_message_router.py::test_pipeline_routes_explicit_agent_through_top_level_prompt -q` 통과
- `uv run --extra dev pytest tests/test_message_router.py tests/test_pipeline_edges.py tests/test_websocket_endpoint.py tests/test_scenario_harness.py -q` 통과: 37 passed
- `uv run --extra dev ruff check .` 통과
- `uv run --extra dev pytest -q` 통과: 86 passed
- 함수/메서드 내부 import 검색 결과 없음
- source 최대 길이: `backend/src/agents/pipeline/runtime.py` 420줄

Reviewer: `Darwin` sub-agent

### 1. 명시 `agent`가 hint only인지 증명하는 회귀 테스트 누락

- severity: medium
- file: `backend/tests/test_message_router.py`

문제:

- 기존 테스트는 명시 `agent`와 모델이 선택한 agent가 같은 경우만 검증했다.
- prompt는 호출하지만 이후 코드가 `envelope.agent`를 우선하는 회귀가 생겨도 테스트가 통과할 수 있다.

필요 작업:

- 요청의 명시 `agent`는 `process_optimizer`지만 모델은 `manual_qa`를 선택하는 테스트를 추가한다.
- 최종 응답이 `manual_qa` 경로를 따른다는 것을 확인한다.

수정:

- `test_pipeline_treats_explicit_agent_as_top_level_prompt_hint_only`를 추가했다.
- 명시 `agent=process_optimizer` 요청에서 모델이 `manual_qa`를 선택하면 최종 응답도 `manual_qa` 경로를 따르는지 검증한다.

### 2. top-level routing parser가 compact JSON 계약보다 느슨함

- severity: medium
- file: `backend/src/agents/orchestrator.py`

문제:

- `parse_agent_route_decision()`이 JSON parsing 실패 시 bare text를 후보로 받아들인다.
- prompt와 문서는 compact JSON 반환을 요구하므로 bare `process_optimizer`가 routing 성공으로 처리되면 계약이 느슨해진다.

필요 작업:

- bare agent id를 거부하는 테스트를 추가한다.
- JSON object의 `agent` 필드만 허용하도록 parser를 수정한다.

수정:

- 처음에는 `parse_agent_route_decision()`이 JSON decode 실패 시 `None`을 반환하도록 수정했다.
- 이후 top-level output 계약을 더 단순화하면서 `parse_agent_route_decision()` 자체를 제거했다.
- [superseded] 당시에는 `parse_sub_agent_route_decision()`을 sub-agent compact JSON 계약으로 유지했다.
- [superseded] 당시에는 bare `process_optimizer`, `manual_qa.machine_help`, `quest_generator.production_quest`를 거부하는 테스트를 추가했다.

추가 수정:

- `parse_agent_selection` / `parse_sub_agent_selection` 명칭이 코드 선택 로직처럼 보여 처음에는 `parse_agent_route_decision` / `parse_sub_agent_route_decision`으로 rename했다.
- [superseded] 이후 top-level은 parser 자체를 제거했고, sub-agent만 `parse_sub_agent_route_decision`을 유지했다.
- `runtime.py`의 top-level local variable도 제거했고, sub-agent local variable만 `sub_agent_route_decision`으로 둔다.
- `route_top_agent` 안의 중복 allowlist 분기를 제거하고, 경로 구분은 `route_selected_agent` conditional edge에 맡겼다.

추가 단순화:

- top-level `parse_agent_route_decision()`도 제거했다.
- `OrchestratorAgent` prompt는 `TOP_LEVEL_AGENT_IDS` 중 하나의 id만 plain string으로 반환하도록 구조화했다.
- `route_top_agent`는 LLM raw decision을 strip해서 `selectedAgent` state에 기록하고, 허용 목록 검증과 구분은 `route_selected_agent` conditional edge가 맡는다.
- JSON 형태의 top-level routing output은 더 이상 허용하지 않고 `ROUTING_UNAVAILABLE`이 되도록 테스트를 추가했다.

재리뷰 finding:

- reviewer: `Descartes` sub-agent
- severity: low
- file: `backend/src/DECISION_LOG.md`
- 문제: 앞쪽 LangGraph 제약 문서가 "LLM 실패는 fallback node로 복구", "fallback schema/validation error만 agent.error"라고 남아 있어 새 top-level routing failure 계약과 충돌했다.

수정:

- LangGraph routing 설명을 `route_top_agent`는 decision state 기록, `route_selected_agent` conditional edge가 실제 경로 구분으로 분리했다.
- top-level routing LLM 실패는 generation fallback 전에 `ROUTING_UNAVAILABLE`로 종료한다고 명시했다.
- Agent 실행 단계 LLM slot 실패만 generation fallback node로 복구한다고 명시했다.

재리뷰 finding:

- reviewer: `Popper` sub-agent
- severity: medium
- file: `backend/src/agents/pipeline/runtime.py`
- 문제: invalid top-level LLM output이 public `agent.error.agent` 필드에 그대로 들어갈 수 있었다.
- 수정: `selectedAgent`가 `TOP_LEVEL_AGENT_IDS` 중 하나일 때만 error envelope의 `agent`로 사용하고, 아니면 요청 envelope의 `agent`로 fallback한다.

재리뷰 finding:

- reviewer: `Popper` sub-agent
- severity: low
- file: `backend/src/DECISION_LOG.md`
- 문제: 문서가 아직 `route_top_agent`가 검증한다고 설명하거나 `orchestrator.py`가 LLM 선택 결과를 파싱한다고 설명했다.
- 수정: `route_top_agent`는 raw decision state 기록, `route_selected_agent` conditional edge가 검증/구분한다고 수정했다.

최종 재리뷰:

- reviewer: `Popper` sub-agent
- result: no unresolved findings
- 확인: `build_agent_error`는 `selectedAgent`가 `TOP_LEVEL_AGENT_IDS`에 있을 때만 public `agent` field에 사용한다.
- 확인: JSON old-contract top-level output은 `ROUTING_UNAVAILABLE`을 반환하면서 요청 `agent` 값을 보존한다.
- 확인: `DECISION_LOG.md`는 `route_top_agent`가 raw decision string을 기록하고 `route_selected_agent`가 검증/구분한다고 설명한다.
- 확인: `uv run --extra dev pytest tests/test_message_router.py::test_pipeline_rejects_json_top_level_routing_output -q` 통과

### 3. 문서에 이전 routing/error 계약이 남아 있음

- severity: low
- file: `backend/src/FOLDER_ROLES.md`, `backend/src/DECISION_LOG.md`

문제:

- 일부 문서는 LLM 실패가 항상 fallback `agent.response`로 복구된다고 설명한다.
- 일부 문서는 명시 `agent` 또는 `sub_agent` 값을 검증 후 사용할 수 있다고 설명한다.
- 새 계약에서는 top-level routing 실패가 `ROUTING_UNAVAILABLE`로 끝나고, 명시 `agent`는 hint only다.

필요 작업:

- top-level routing failure는 fallback 전에 `agent.error`가 될 수 있음을 문서에 명시한다.
- 명시 `agent`와 명시 `sub_agent`의 계약을 분리해서 설명한다.

수정:

- `FOLDER_ROLES.md`에 top-level routing failure는 fallback 전에 `ROUTING_UNAVAILABLE`이 될 수 있음을 명시했다.
- `DECISION_LOG.md`에서 top-level `agent`는 hint only, `sub_agent`는 top-level Agent 확정 후 도메인 허용 목록으로 검증하는 계약으로 분리했다.
- `llm_implementation_sprint.md`의 Sprint 5.1 acceptance를 새 top-level routing 계약에 맞게 수정했다.

최종 재리뷰:

- reviewer: `Darwin` sub-agent
- result: no unresolved findings
- 확인: 명시 `agent`와 모델 선택이 다른 경우를 테스트가 고정한다.
- [superseded] 확인: 당시 계약은 top-level plain Agent id 문자열, sub-agent compact JSON parser였다. 현재 계획은 sub-agent도 plain id 문자열 계약으로 바뀌었다.
- 확인: 문서가 top-level `agent` hint only와 `ROUTING_UNAVAILABLE` 계약을 반영한다.
- 확인: `uv run --extra dev pytest tests/test_message_router.py tests/test_agent_contracts.py tests/test_pipeline_edges.py tests/test_websocket_endpoint.py tests/test_scenario_harness.py -q` 통과: 41 passed

### 4. LangGraph 구성도와 review 기록이 structured prompt 계약과 불일치

- reviewer: `Carson` sub-agent
- severity: low
- file: `backend/src/FOLDER_ROLES.md`, `_workspace/factory-agent/review-feedback.md`

문제:

- `FOLDER_ROLES.md`의 LangGraph 구성도가 `route_top_agent`를 top-level branching node처럼 표시했다.
- 실제 구현은 `route_top_agent`가 prompt를 호출하고 `selectedAgent`를 기록한 뒤, `route_selected_agent` conditional edge가 검증과 분기를 담당한다.
- state 목록에 `routingPrompt`, `routingRaw`가 빠져 있었다.
- 이전 review 기록이 top-level routing parser도 JSON decode 실패 시 bare text를 거부한다고 적어 현재 계약과 맞지 않았다.

수정:

- LangGraph 구성도에 `route_selected_agent` conditional edge를 명시했다.
- sub-agent routing 뒤에는 leaf node가 아니라 `route_sub_agent_result`를 거쳐 `cache_lookup`으로 진행한다고 정리했다.
- state 목록에 `routingPrompt`, `routingRaw`, `responseEnvelope`을 추가했다.
- [superseded] 당시 review 기록은 top-level plain Agent id 문자열 계약, sub-agent parser compact JSON 계약으로 분리했다. 현재 계획은 sub-agent도 plain id 문자열 계약으로 바뀌었다.

최종 재리뷰:

- reviewer: `Carson` sub-agent
- result: no findings
- 확인: `FOLDER_ROLES.md`는 `route_top_agent`를 일반 node로, `route_selected_agent`를 top-level 검증/분기 conditional edge로 표현한다.
- 확인: state 목록은 `routingPrompt`, `routingRaw`, `responseEnvelope`을 포함한다.
- [superseded] 확인: 당시 review 기록은 top-level plain Agent id output과 sub-agent compact JSON parser 동작을 구분했다. 현재 계획은 sub-agent도 plain id 문자열 계약으로 바뀌었다.

### 5. 상위 Agent 처리 기준 문서 리뷰

- reviewer: `Poincare` sub-agent
- severity: low
- file: `backend/src/DECISION_LOG.md`, `backend/src/AGENT_ROLES.md`, `backend/src/FOLDER_ROLES.md`

문제:

- `DECISION_LOG.md`가 LangGraph routing node를 오케스트레이터 내부 실행 단계처럼 설명했다.
- `AGENT_ROLES.md`가 leaf Agent를 "바로 prompt 생성, response parsing, fallback을 수행"한다고 설명해 pipeline 실행 책임과 섞일 수 있었다.
- `FOLDER_ROLES.md` 구성도에서 leaf top-level Agent도 `route_sub_agent_result`를 거쳐 sub-agent routing이 있는 것처럼 오해될 수 있었다.

수정:

- LangGraph routing node는 오케스트레이터 내부 구현이 아니라 `agents/pipeline/`이 소유하는 실행 node라고 정리했다.
- leaf Agent는 `build_prompt()`, response schema, `fallback()` 정책을 제공하고 pipeline이 이를 실행한다고 수정했다.
- `route_sub_agent_result`는 sub-agent 전용이 아니라 `selectedSubAgent` 또는 `error` state를 확인하는 공통 validity/error edge라고 설명했다.
- 사용자의 "중간 에이전트" 용어 질문에 맞춰 공식 용어를 `Domain Orchestrator` / `도메인 오케스트레이터`로 정했다.
- active 문서의 `도메인 서브 오케스트레이터`, `서브 오케스트레이터` 표현을 `도메인 오케스트레이터`로 통일했다.

재리뷰 finding:

- reviewer: `Poincare` sub-agent
- severity: low
- file: `backend/src/AGENT_ROLES.md`, `backend/src/DECISION_LOG.md`
- 문제: 공식 용어로 쓰지 않기로 한 `상위 Agent` 표현이 active 문서의 제목과 본문에 남아 있었다.
- 수정: 해당 표현을 `top-level Agent` 또는 `도메인 오케스트레이터`로 바꿨고, `중간 에이전트` 같은 계층 위치 표현은 공식 용어로 쓰지 않는다고 정리했다.

최종 재리뷰:

- reviewer: `Poincare` sub-agent
- result: no findings
- 확인: active 문서에서 `상위 Agent`는 `top-level Agent` 또는 `도메인 오케스트레이터`로 정리됐다.
- 확인: active 문서에 `도메인 서브 오케스트레이터` 표현은 남아 있지 않다.
- 확인: `Global Orchestrator` / `Domain Orchestrator` / `Leaf Agent` 경계가 일관적이다.

### 6. 기획 문서 routing/generation 계약 불일치

- reviewer: `Laplace` sub-agent
- severity: planning
- file: `backend/docs/plans/*.md`, `backend/src/DECISION_LOG.md`, `backend/src/AGENT_ROLES.md`

문제:

- top-level routing은 structured prompt와 plain id output으로 바뀌었지만, sub-agent routing 계획에는 compact JSON/parser 계약이 남아 있었다.
- LLM 구현 계획은 raw text를 항상 JSON parsing 대상으로 설명해 routing prompt의 plain id 계약과 충돌했다.
- 오래된 server/pipeline 계획은 LLM 실패가 항상 fallback `agent.response`라고 설명해 routing failure의 `ROUTING_UNAVAILABLE` 계약과 충돌했다.

수정:

- routing prompt는 top-level/sub-agent 모두 허용 id 문자열 하나만 반환한다고 계획을 정리했다.
- leaf Agent generation prompt만 Agent별 response JSON object를 반환한다고 분리했다.
- routing id 검증은 LangGraph conditional edge/node가 담당하고, generation JSON 검증은 pipeline parser/schema node가 담당한다고 정리했다.
- routing LLM decision 실패는 deterministic fallback agent를 고르지 않고 `ROUTING_UNAVAILABLE`로 종료한다고 오래된 계획 문서에도 반영했다.

기획 리뷰 finding:

- reviewer: `Laplace` sub-agent
- severity: high
- file: `backend/docs/plans/llm_implementation_plan.md`
- 문제: provider-level JSON response mode가 routing plain id 계약과 충돌했다.
- 수정: routing 호출에는 JSON response mode를 설정하지 않고, generation 호출에만 provider JSON mode를 적용한다고 분리했다.

기획 리뷰 finding:

- reviewer: `Laplace` sub-agent
- severity: medium
- file: `backend/docs/plans/llm_implementation_plan.md`, `backend/docs/plans/llm_implementation_sprint.md`, `backend/docs/plans/agent_pipeline_implementation_plan.md`, `backend/docs/plans/server_implementation_plan.md`
- 문제: deterministic fallback이 routing/generation 구분 없이 설명됐다.
- 수정: generation LLM 실패만 deterministic fallback으로 복구하고, routing decision 실패는 `ROUTING_UNAVAILABLE`로 종료한다고 범위를 좁혔다.

기획 리뷰 finding:

- reviewer: `Laplace` sub-agent
- severity: medium
- file: `backend/docs/plans/agent_pipeline_implementation_plan.md`
- 문제: pipeline 단계와 LLM adapter 계획이 old explicit-agent execution, `generate_json`, adapter dict return 기준으로 남아 있었다.
- 수정: prompt-based routing, raw text adapter, LangGraph conditional edge 검증, generation JSON parse 순서로 갱신했다.

기획 리뷰 finding:

- reviewer: `Laplace` sub-agent
- severity: low
- file: `backend/docs/plans/server_implementation_plan.md`
- 문제: `Agent prompt는 JSON object만 출력`이라는 일반 규칙이 routing prompt와 충돌했다.
- 수정: routing prompt는 허용 id 문자열, leaf generation prompt는 JSON object로 분리했다.

재리뷰 finding:

- reviewer: `Laplace` sub-agent
- severity: medium
- file: `backend/docs/plans/llm_implementation_plan.md`, `backend/docs/plans/llm_implementation_sprint.md`
- 문제: LLM plan/sprint에서 deterministic fallback이 call type 구분 없이 generic terminal path처럼 남아 있었다.
- 수정: routing은 `default -> fallback1 -> fallback2 -> ROUTING_UNAVAILABLE`, generation은 `default -> fallback1 -> fallback2 -> deterministic fallback`으로 분리했다.

최종 재리뷰 finding:

- reviewer: `Laplace` sub-agent
- severity: low
- file: `backend/docs/plans/llm_implementation_plan.md`, `backend/docs/plans/llm_implementation_sprint.md`
- 문제: LLM slot 순서 목록과 sprint step에 generation 전용 fallback 표시가 빠져 있었다.
- 수정: LLM slot 순서 목록을 call type별 terminal path로 바꾸고, sprint step에도 generation fallback임을 명시했다.

최종 재리뷰:

- reviewer: `Laplace` sub-agent
- result: no findings
- 확인: `llm_implementation_plan.md`와 sprint 모두 routing/generation terminal path가 분리됐다.
- 확인: provider JSON mode는 generation 전용이고 routing은 plain id 계약이다.
- 확인: old `generate_json`, adapter dict return, explicit `lookup(agent)` 계획 잔여물은 active planning 범위에서 발견되지 않았다.

재리뷰 finding:

- reviewer: `Laplace` sub-agent
- severity: low
- file: `_workspace/factory-agent/review-feedback.md`
- 문제: planning review section의 reviewer 상태가 `pending`으로 남아 있었다.
- 수정: reviewer를 `Laplace` sub-agent로 기록했다.

## 2026-05-31 Sub-agent routing parser 제거

### 1. 사용자 리뷰 finding: sub-agent parser 잔존

- reviewer: user
- severity: high
- file: `backend/src/agents/manual_qa/agent.py`, `backend/src/agents/quest_generator/agent.py`, `backend/src/agents/pipeline/runtime.py`

문제:

- `parse_sub_agent_route_decision()`가 `manual_qa`와 `quest_generator`에 남아 있었다.
- 해당 함수는 compact JSON을 decode하고 `candidate in *_SUB_AGENT_IDS`로 검증해, sub-agent 선택을 다시 코드 로직에 태웠다.
- 최신 계약은 top-level/sub-agent 모두 structured prompt가 허용 id 문자열 하나를 반환하고, LangGraph conditional edge가 경로를 구분하는 방식이다.

수정:

- `parse_sub_agent_route_decision()`를 두 도메인 오케스트레이터에서 제거했다.
- domain leaf routing prompt를 `[ROLE]`, `[TASK]`, `[ALLOWED_LEAF_AGENT_IDS]`, `[REQUEST_CONTEXT]`, `[REQUEST_PAYLOAD]`, `[OUTPUT_CONTRACT]` 구조로 바꿨다.
- `manual_qa.route_sub_agent`, `quest_generator.route_sub_agent` node는 LLM raw output을 trim해 `selectedLeafAgent` state에 기록만 하도록 바꿨다.
- `route_selected_leaf_agent` conditional edge가 선택된 top-level Agent별 허용 leaf Agent id를 검증하도록 수정했다.

### 2. 사용자 리뷰 finding: 용어 혼동

- reviewer: user
- severity: medium
- file: `backend/src/agents/pipeline/state.py`, `backend/src/agents/pipeline/graph_edges.py`, `backend/src/agents/pipeline/runtime.py`, `backend/src/DECISION_LOG.md`

문제:

- 실행 대상 Agent를 `selectedSubAgent`로 부르면 `process_optimizer`, `new_material_generator` 같은 leaf top-level Agent도 sub-agent처럼 보인다.
- 공통 conditional edge 이름이 `route_sub_agent_result`라서 domain sub-agent 전용 edge처럼 보였다.

수정:

- pipeline state와 response metadata를 `selectedLeafAgent`로 바꿨다.
- 공통 conditional edge predicate를 `route_selected_leaf_agent`로 바꿨다.
- 정확한 용어를 `DECISION_LOG.md`에 추가했다: `selectedAgent`, `selectedLeafAgent`, request payload의 `sub_agent`, `route_selected_agent`, `route_selected_leaf_agent`.

### 3. 리뷰 전용 에이전트 finding: routing behavior 범위 확인

- reviewer: `Mill` sub-agent
- severity: high
- file: `backend/src/agents/pipeline/runtime.py`, `backend/tests/test_message_router.py`

검토:

- reviewer는 explicit `agent`를 코드에서 확정하지 않고 routing prompt를 항상 호출하는 변경을 public behavior regression 가능성으로 지적했다.
- 이 변경은 사용자의 기존 지시인 "오케스트레이터가 구분은 프롬프트로 해야 한다", "에이전트 쓰는 곳은 로직을 쓰지마", "명시 agent는 hint" 계약과 일치하므로 rollback하지 않는다.
- 해당 계약은 `test_pipeline_routes_explicit_agent_through_top_level_prompt`, `test_pipeline_treats_explicit_agent_as_top_level_prompt_hint_only`, `DECISION_LOG.md`에 고정되어 있다.

### 4. 리뷰 전용 에이전트 finding: leaf top-level metadata 테스트 보강

- reviewer: `Mill` sub-agent
- severity: medium
- file: `backend/tests/harness.py`, `backend/tests/test_message_router.py`

문제:

- `selectedLeafAgent` metadata 검증이 sub-agent 인자를 넘긴 경우에만 수행되어 `process_optimizer` 같은 leaf top-level Agent 응답의 metadata rename 회귀를 놓칠 수 있었다.

수정:

- 공통 `assert_agent_response()`가 모든 성공 응답에서 `selectedLeafAgent == (sub_agent or agent)`를 검증하도록 강화했다.
- 모든 성공 응답에서 `selectedSubAgent`가 metadata에 남지 않는지도 검증한다.

### 5. 리뷰 전용 에이전트 finding: 문서 용어 잔여물

- reviewer: `Mill` sub-agent
- severity: low
- file: `backend/docs/plans/agent_pipeline_implementation_plan.md`, `backend/src/DECISION_LOG.md`

문제:

- pipeline 단계 문서가 `selectedLeafAgent` 기록 후에도 "sub-agent route 검증"이라고 설명했다.
- cache key 결정 로그가 실제 코드의 `leaf_agent` 키와 달리 `sub_agent`를 포함한다고 설명했다.

수정:

- pipeline 단계 설명을 "selected leaf Agent 검증"으로 바꿨다.
- cache key 결정 로그를 `agent`, `leaf_agent`, `payload`, context metadata 기준으로 바꿨다.

### 6. 재리뷰 finding: cache hit metadata 테스트 누락

- reviewer: `McClintock` sub-agent
- severity: medium
- file: `backend/tests/test_pipeline_edges.py`, `backend/tests/test_message_router.py`

문제:

- cache hit/context cache 성공 응답 테스트가 공통 `assert_agent_response()`를 거치지 않아 `selectedLeafAgent`와 `selectedSubAgent` 부재 검증을 놓칠 수 있었다.
- 특히 cache hit 경로는 metadata를 재조립하므로 별도 검증이 필요하다.

수정:

- cache context 분리 테스트와 cache hit 테스트에 `assert_agent_response()`를 추가했다.
- 이로써 cache hit/miss 성공 응답 모두 `selectedLeafAgent` metadata와 `selectedSubAgent` 부재를 검증한다.

### 7. 재리뷰 finding: cache 정책 문서 불일치

- reviewer: `McClintock` sub-agent
- severity: low
- file: `backend/docs/plans/agent_pipeline_implementation_plan.md`

문제:

- 계획 문서의 cache key가 `agent + snapshotHash + normalizedPayloadHash`로 남아 있었지만, 실제 코드는 `agent`, `leaf_agent`, `payload`, `session_id`, `client_id`, `context.metadata`를 포함한다.
- 문서가 cache hit metadata를 `metadata.cacheHit = true`로 설명했지만, 실제 코드는 `metadata.cache = "hit"`을 사용한다.

수정:

- cache key 문서를 실제 코드의 `agent + leaf_agent + payload + session_id + client_id + context.metadata` 기준으로 수정했다.
- cache hit metadata 문서를 `metadata.cache = "hit"`로 수정하고, cache miss는 별도 cache metadata를 추가하지 않는다고 명시했다.
- 같은 구식 cache key 표현이 남아 있던 `FOLDER_ROLES.md`, `server_implementation_plan.md`도 실제 코드 기준으로 정리했다.

### 8. 최종 재리뷰

- reviewer: `Russell` sub-agent
- result: no findings
- 확인: cache hit/context cache 성공 응답 테스트가 `selectedLeafAgent`와 `selectedSubAgent` 부재를 검증한다.
- 확인: cache key/cache hit metadata 문서가 실제 코드와 일치한다.
- 확인: 코드 active path에 `parse_sub_agent*`, `selectedSubAgent`, `route_sub_agent_result`, `[ALLOWED_SUB_AGENT_IDS]`가 남아 있지 않다.

## 2026-06-17 Quest MVP Phase 5 Review

Status: resolved

Reviewer: `reviewer` sub-agent (ID: `70fc8586-63f6-4d77-9fb4-faf2582b85a9`)

### 1. 상태 머신 전이 및 멱등성 검증 누락

- severity: major
- file: `backend/src/agents/quest_generator/manager.py`

문제:

- `complete_quest` 및 `claim_reward` 호출 시 대상 퀘스트의 현재 상태를 검증하지 않고 덮어썼다.
- 플레이어가 퀘스트를 완료하지 않은 상태에서 `claim_reward`를 바로 호출하여 치팅이 가능했고, 중복 완료 시 `completed_at`이 새로 갱신되는 멱등성 위반 문제가 존재했다.

수정:

- `complete_quest` 메서드 내에 멱등성 가드를 추가해 상태가 `in_progress`가 아닌 경우 데이터 변경 없이 기존 모델의 Pydantic 뷰를 즉시 조기 반환하도록 수정했다.
- `claim_reward` 메서드 내에 상태 제약을 추가해 상태가 `completed`가 아닌 경우 상태 전이를 차단하고 `None`을 반환하게 처리했다.

### 2. 부정 상태 전이 테스트 누락

- severity: minor
- file: `backend/tests/test_quest_manager.py`

문제:

- 정상 완료 및 보상 획득 시나리오만 검증되고 있고, 비정상적 상태 전이 시도 및 멱등성(중복 호출 시 시간 고정) 검증이 누락되어 있었다.

수정:

- `test_invalid_state_transitions` 부정 테스트 케이스를 신규 추가하여 `in_progress -> reward_claimed` 차단 확인, 중복 `complete_quest` 호출 시 `completed_at` 시간 고정(멱등성) 등을 검증했다.

### 3. QuestRewardResolver 방어 코드 누락

- severity: minor
- file: `backend/src/agents/quest_generator/reward_resolver.py`

문제:

- `target_item_id`가 빈 문자열(`""`)로 올 경우 불필요한 레시피 맵 DFS 탐색이 발생한다.

수정:

- `resolve_rewards` 최상단에 `if not target_item_id: return rewards` 가드를 추가하고, `test_reward_resolver_empty_target`을 추가해 검증했다.

### 4. 최종 재리뷰

- reviewer: `reviewer` sub-agent
- result: no findings
- 확인: `test_invalid_state_transitions` 부정 테스트가 멱등성과 비정상 전이 차단을 철저하게 검증한다.
- 확인: `resolve_rewards`에 빈 타겟 방어 코드가 올바르게 작동한다.
