# LLM 구현 Sprint

이 파일은 `backend/docs/plans/llm_implementation_plan.md`를 실행하기 위한 sprint 체크리스트다.

## Sprint 목표

실제 LLM provider를 backend agent pipeline에 연결한다. `llm` 패키지는 provider 설정과 1회 호출 adapter만 담당하고, `default -> fallback1 -> fallback2 -> deterministic fallback` 순서는 LangGraph pipeline에서 제어한다.

## Sprint 1: Pipeline edge 선행 수정

Status: completed

### Task 1.1 cache metadata 보존

Files:

- `backend/tests/test_pipeline_edges.py`
- `backend/src/cache/response_cache.py`
- `backend/src/agents/pipeline/`

Steps:

- [x] `test_pipeline_cache_hit_preserves_original_response_metadata` 실패 테스트 추가
- [x] `uv run --extra dev pytest tests/test_pipeline_edges.py::test_pipeline_cache_hit_preserves_original_response_metadata -q`로 RED 확인
- [x] cache entry에 `payload`와 `metadata`를 함께 저장하도록 `ResponseCache` 수정
- [x] cache hit에서 기존 metadata를 유지하고 `cache: hit`만 추가
- [x] 동일 테스트 GREEN 확인
- [x] 커밋: `b3c7f1e fix: cache hit metadata 보존`

### Task 1.2 malformed envelope correlation 보존

Files:

- `backend/tests/test_pipeline_edges.py`
- `backend/src/agents/pipeline/`

Steps:

- [x] `test_pipeline_validation_error_preserves_raw_correlation_fields` 실패 테스트 추가
- [x] `uv run --extra dev pytest tests/test_pipeline_edges.py::test_pipeline_validation_error_preserves_raw_correlation_fields -q`로 RED 확인
- [x] `_build_validation_error()`가 raw message에서 correlation field를 복구하도록 수정
- [x] 동일 테스트 GREEN 확인
- [x] 커밋: `7d09463 fix: validation error correlation 보존`

## Sprint 2: LLM slot 설정

Status: completed

### Task 2.1 settings 모델 추가

Files:

- `backend/src/llm/settings.py`
- `backend/tests/test_llm_settings.py`
- `backend/.env.example`

Steps:

- [x] default/fallback1/fallback2 slot 기본값 테스트 작성
- [x] `ENVIRONMENT=development`에서 default slot이 local provider를 사용하는 테스트 작성
- [x] provider 검증 테스트 작성
- [x] provider별 API key alias 우선순위 테스트 작성
- [x] local provider는 `BASE_URL`을 요구하는 테스트 작성
- [x] `uv run --extra dev pytest tests/test_llm_settings.py -q`로 RED 확인
- [x] `LLMSettings.from_env()` 구현
- [x] `.env.example`을 `FACTORY_LLM_*` 기준으로 갱신
- [x] `.env.example`의 development 예시는 local LLM을 기본 provider로 둔다.
- [x] settings 테스트 GREEN 확인
- [x] 커밋: `a5aa46a test: LLM 설정 계약 추가`
- [x] 리뷰 반영: `abaef31 chore: LLM 의존성 정리 및 리뷰 반영`

Acceptance:

- env 미설정과 CI 기본 slot provider는 모두 `none`
- dev 모드 기본 slot은 `default=local`, `fallback1=none`, `fallback2=none`
- slot 이름은 `default`, `fallback1`, `fallback2`
- `google` slot 기본 model은 `gemini-2.5-flash`
- `openai` slot은 model을 명시해야 한다.
- `local` slot은 model과 base URL을 명시해야 한다.
- timeout 기본값은 `20000ms`
- `google` key 우선순위는 slot key, `GEMINI_API_KEY`, `GOOGLE_API_KEY`
- `openai` key 우선순위는 slot key, `OPENAI_API_KEY`
- `local` key는 선택값이다.

## Sprint 3: 의존성 정리

Status: completed

### Task 3.1 provider SDK 정리

Files:

- `backend/pyproject.toml`
- `backend/uv.lock`

Steps:

- [x] `rg -n "google-generativeai|google.generativeai|langchain-google-genai|langchain" backend/src backend/tests backend/pyproject.toml` 실행
- [x] 기존 미사용 LLM 의존성 제거
- [x] `google-genai>=1.33.0` 추가
- [x] OpenAI-compatible HTTP 호출은 별도 의존성 없이 표준 라이브러리 기반으로 진행
- [x] FastAPI 범위를 최신 0.136 patch line인 `fastapi>=0.136.3,<0.137.0`로 축소
- [x] `uv lock` 실행
- [x] `uv run --extra dev pytest tests/test_llm_settings.py -q` 실행
- [x] 커밋: `abaef31 chore: LLM 의존성 정리 및 리뷰 반영`

Acceptance:

- backend runtime code가 deprecated `google-generativeai`에 의존하지 않는다.
- lock file이 갱신된다.

## Sprint 4: Adapter 구현

Status: in progress

### Task 4.1 Noop adapter와 slot factory

Files:

- `backend/src/llm/adapter.py`
- `backend/tests/test_llm_adapter.py`

Steps:

- [x] `NoopLLMAdapter.invoke()`가 `None`을 반환하는 테스트 작성
- [x] `none` provider slot은 `NoopLLMAdapter`를 반환하는 테스트 작성
- [x] Google slot은 `GoogleGenAiLLMAdapter`를 반환하는 테스트 작성
- [x] OpenAI slot은 `OpenAILLMAdapter`를 반환하는 테스트 작성
- [x] Local slot은 `LocalLLMAdapter`를 반환하는 테스트 작성
- [x] `uv run --extra dev pytest tests/test_llm_adapter.py -q`로 RED 확인
- [x] `LLMAdapter` protocol, `NoopLLMAdapter`, `create_llm_adapter(slot)` 구현
- [x] adapter 테스트 GREEN 확인
- [x] 커밋: `e5021ed test: LLM slot adapter 계약 추가`

### Task 4.2 Google Gen AI adapter

Files:

- `backend/src/llm/adapter.py`
- `backend/tests/test_llm_adapter.py`

Steps:

- [x] fake Google client로 성공 응답 테스트 작성
- [x] 빈 응답은 `None`으로 변환하는 테스트 작성
- [x] provider 예외는 `None`으로 변환하는 테스트 작성
- [x] `uv run --extra dev pytest tests/test_llm_adapter.py::test_google_llm_adapter_returns_response_text -q`로 RED 확인
- [x] `GoogleGenAiLLMAdapter` 구현
- [x] `response_mime_type="application/json"` 설정
- [x] `max_output_tokens`, `temperature`, `timeout_ms` 전달
- [x] adapter 테스트 GREEN 확인
- [x] 커밋: `feat: Google Gen AI LLM adapter 추가`

Acceptance:

- unit test는 실제 Google API를 호출하지 않는다.
- provider 실패는 예외 전파가 아니라 `None` 반환이다.

### Task 4.3 OpenAI-compatible adapter

Files:

- `backend/src/llm/adapter.py`
- `backend/tests/test_llm_adapter.py`

Steps:

- [x] fake HTTP client로 GPT/OpenAI-compatible 성공 응답 테스트 작성
- [x] API key 없음은 `None` 반환 테스트 작성
- [x] provider error는 `None` 반환 테스트 작성
- [x] JSON object raw text를 그대로 반환하는 테스트 작성
- [x] adapter 구현
- [x] adapter 테스트 GREEN 확인
- [x] 커밋: `feat: OpenAI-compatible LLM adapter 추가`

Acceptance:

- model 이름은 설정에서만 온다.
- API key는 slot key 또는 `OPENAI_API_KEY`에서 온다.
- 실제 OpenAI API는 unit test에서 호출하지 않는다.
- OpenAI-compatible 호출은 Chat Completions 형식인 `POST /v1/chat/completions`를 사용한다.

### Task 4.4 Local OpenAI-compatible adapter

Files:

- `backend/src/llm/adapter.py`
- `backend/tests/test_llm_adapter.py`

Steps:

- [x] local base URL 누락 정책을 settings와 정합화한다. 현재 `LLMSettings`는 local provider에 model/base URL을 요구한다.
- [x] fake local endpoint 성공 응답 테스트 작성
- [x] local endpoint error는 `None` 반환 테스트 작성
- [x] adapter 구현
- [x] adapter 테스트 GREEN 확인
- [x] 커밋: `feat: local LLM adapter 추가`

Acceptance:

- 로컬 LLM은 API key 없이도 동작할 수 있다.
- endpoint/base URL은 slot 설정으로만 주입한다.
- OpenAI-compatible 호출은 Chat Completions 형식인 `POST /v1/chat/completions`를 사용한다.

## Sprint 5: LangGraph fallback wiring

### Task 5.1 LLM fallback node/edge 연결

Files:

- `backend/src/agents/pipeline/`
- `backend/tests/test_pipeline_edges.py`

Steps:

- [x] `AgentPipeline()` 기본값이 API 없이 fallback 응답을 반환하는 테스트 추가
- [x] 기존 placeholder 의존 상태에서 테스트 baseline 확인
- [x] `AgentPipeline._build_graph()`가 settings에서 default/fallback1/fallback2 adapter를 만든다.
- [x] `call_llm.default`, `call_llm.fallback1`, `call_llm.fallback2` node를 추가한다.
- [x] default가 `None`이면 fallback1 node로 이동하는 conditional edge를 추가한다.
- [x] fallback1이 `None`이면 fallback2 node로 이동하는 conditional edge를 추가한다.
- [x] fallback2가 `None`이면 deterministic fallback node로 이동하는 conditional edge를 추가한다.
- [x] 성공한 LLM slot/provider/model을 response metadata에 남긴다.
- [x] `uv run --extra dev pytest tests/test_pipeline_edges.py -q` 실행
- [x] 커밋: `feat: LangGraph LLM fallback 경로 연결`

Acceptance:

- `llm`을 명시 주입하는 기존 테스트는 계속 외부 API를 호출하지 않는다.
- `llm` 미주입 기본 pipeline은 settings 기준 default/fallback1/fallback2 adapter를 생성한다.
- LLM fallback 순서는 adapter가 아니라 LangGraph node/edge 테스트로 검증된다.

### Task 5.2 LLM fallback 순서 테스트

Files:

- `backend/tests/test_pipeline_edges.py`

Steps:

- [ ] default가 실패하고 fallback1이 성공하면 fallback2를 호출하지 않는 테스트 작성
- [ ] default와 fallback1이 실패하고 fallback2가 성공하는 테스트 작성
- [ ] 세 slot이 모두 실패하면 deterministic fallback response가 반환되는 테스트 작성
- [ ] 성공 응답 metadata에 `llmSlot`, `llmProvider`, `llmModel`이 들어가는 테스트 작성
- [ ] `uv run --extra dev pytest tests/test_pipeline_edges.py -q` 실행
- [ ] 커밋: `test: LangGraph LLM fallback 순서 고정`

## Sprint 6: Prompt routing 경로 보강

### Task 6.1 LLM decision 기반 routing 테스트

Files:

- `backend/tests/test_pipeline_edges.py`

Steps:

- [ ] top-level orchestrator가 LLM raw JSON으로 `manual_qa`를 선택하는 테스트 추가
- [ ] `quest_generator`가 LLM raw JSON으로 `quest_generator.production_quest`를 선택하는 테스트 추가
- [ ] invalid routing JSON이 `ROUTING_UNAVAILABLE`로 종료되는 테스트 추가
- [ ] `uv run --extra dev pytest tests/test_pipeline_edges.py -q` 실행
- [ ] 커밋: `test: LLM 기반 agent routing 경로 보강`

Acceptance:

- keyword routing 로직 없이 LLM decision 경로가 검증된다.
- invalid decision은 임의 fallback agent를 고르지 않는다.

## Sprint 7: 문서 정리

### Task 7.1 운영 문서와 결정 로그 갱신

Files:

- `backend/src/DECISION_LOG.md`
- `backend/README.md`
- `backend/docs/plans/llm_implementation_plan.md`
- `backend/docs/plans/llm_implementation_sprint.md`

Steps:

- [ ] `DECISION_LOG.md`에 `google-genai` 선택 이유 기록
- [ ] `README.md`에 CI/test에서 모든 slot이 `none`인 실행 예시 추가
- [ ] `README.md`에 dev 모드 local LLM 기본 실행 예시 추가
- [ ] `README.md`에 Google 기본 모델, OpenAI fallback1, local fallback2 예시 추가
- [ ] `rg -n "OpenAI API Configuration|google-generativeai|langchain-google-genai" backend`로 낡은 설정 문구 확인
- [ ] 커밋: `docs: LLM provider 운영 규칙 정리`

## 최종 검증

Steps:

- [ ] `uv run --extra dev pytest`
- [ ] `uv run --extra dev ruff check .`
- [ ] `FACTORY_LLM_DEFAULT_PROVIDER=none FACTORY_LLM_FALLBACK1_PROVIDER=none FACTORY_LLM_FALLBACK2_PROVIDER=none uv run --extra dev pytest tests/test_pipeline_edges.py -q`
- [ ] `git status --short`
- [ ] `git push -u origin docs/llm-adapter-implementation`

완료 기준:

- 전체 테스트 통과
- Ruff 통과
- 외부 API key 없이 테스트 통과
- LangGraph default/fallback1/fallback2 fallback 테스트 통과
- sprint별 커밋 분리 완료
