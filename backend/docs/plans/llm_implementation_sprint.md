# LLM 구현 Sprint

이 파일은 `backend/docs/plans/llm_implementation_plan.md`를 실행하기 위한 sprint 체크리스트다.

## Sprint 목표

실제 LLM provider를 backend agent pipeline에 연결한다. 기본 실행은 외부 API 없이 fallback으로 동작하고, Google Gen AI는 설정이 있을 때만 호출한다.

## Sprint 1: Pipeline edge 선행 수정

### Task 1.1 cache metadata 보존

Files:

- `backend/tests/test_pipeline_edges.py`
- `backend/src/cache/response_cache.py`
- `backend/src/agents/pipeline.py`

Steps:

- [ ] `test_pipeline_cache_hit_preserves_original_response_metadata` 실패 테스트 추가
- [ ] `uv run --extra dev pytest tests/test_pipeline_edges.py::test_pipeline_cache_hit_preserves_original_response_metadata -q`로 RED 확인
- [ ] cache entry에 `payload`와 `metadata`를 함께 저장하도록 `ResponseCache` 수정
- [ ] cache hit에서 기존 metadata를 유지하고 `cache: hit`만 추가
- [ ] 동일 테스트 GREEN 확인
- [ ] 커밋: `fix: cache hit metadata 보존`

### Task 1.2 malformed envelope correlation 보존

Files:

- `backend/tests/test_pipeline_edges.py`
- `backend/src/agents/pipeline.py`

Steps:

- [ ] `test_pipeline_validation_error_preserves_raw_correlation_fields` 실패 테스트 추가
- [ ] `uv run --extra dev pytest tests/test_pipeline_edges.py::test_pipeline_validation_error_preserves_raw_correlation_fields -q`로 RED 확인
- [ ] `_build_validation_error()`가 raw message에서 correlation field를 복구하도록 수정
- [ ] 동일 테스트 GREEN 확인
- [ ] 커밋: `fix: validation error correlation 보존`

## Sprint 2: LLM 설정

### Task 2.1 settings 모델 추가

Files:

- `backend/src/llm/settings.py`
- `backend/tests/test_llm_settings.py`
- `backend/.env.example`

Steps:

- [ ] settings 기본값 테스트 작성
- [ ] provider 검증 테스트 작성
- [ ] API key alias 우선순위 테스트 작성
- [ ] `uv run --extra dev pytest tests/test_llm_settings.py -q`로 RED 확인
- [ ] `LlmSettings.from_env()` 구현
- [ ] `.env.example`을 `FACTORY_LLM_*` 기준으로 갱신
- [ ] settings 테스트 GREEN 확인
- [ ] 커밋: `test: LLM 설정 계약 추가`

Acceptance:

- 기본 provider는 `none`
- 기본 model은 `gemini-2.5-flash`
- timeout 기본값은 `20000ms`
- key 우선순위는 `FACTORY_LLM_API_KEY`, `GEMINI_API_KEY`, `GOOGLE_API_KEY`

## Sprint 3: 의존성 정리

### Task 3.1 Google Gen AI SDK 전환

Files:

- `backend/pyproject.toml`
- `backend/uv.lock`

Steps:

- [ ] `rg -n "google-generativeai|google.generativeai|langchain-google-genai|langchain" backend/src backend/tests backend/pyproject.toml` 실행
- [ ] 기존 미사용 LLM 의존성 제거
- [ ] `google-genai>=1.33.0` 추가
- [ ] `uv lock` 실행
- [ ] `uv run --extra dev pytest tests/test_llm_settings.py -q` 실행
- [ ] 커밋: `chore: Google Gen AI SDK 의존성 정리`

Acceptance:

- backend runtime code가 deprecated `google-generativeai`에 의존하지 않는다.
- lock file이 갱신된다.

## Sprint 4: Adapter 구현

### Task 4.1 Noop adapter와 factory

Files:

- `backend/src/llm/adapter.py`
- `backend/tests/test_llm_adapter.py`

Steps:

- [ ] `NoopLlmAdapter.invoke()`가 `None`을 반환하는 테스트 작성
- [ ] 기본 settings에서 `create_llm_adapter()`가 noop을 반환하는 테스트 작성
- [ ] Google provider지만 API key가 없으면 noop을 반환하는 테스트 작성
- [ ] `uv run --extra dev pytest tests/test_llm_adapter.py -q`로 RED 확인
- [ ] `LlmAdapter` protocol, `NoopLlmAdapter`, `create_llm_adapter()` 구현
- [ ] adapter 테스트 GREEN 확인
- [ ] 커밋: `test: LLM noop adapter 계약 추가`

### Task 4.2 Google Gen AI adapter

Files:

- `backend/src/llm/adapter.py`
- `backend/tests/test_llm_adapter.py`

Steps:

- [ ] fake Google client로 성공 응답 테스트 작성
- [ ] 빈 응답은 `None`으로 변환하는 테스트 작성
- [ ] provider 예외는 `None`으로 변환하는 테스트 작성
- [ ] `uv run --extra dev pytest tests/test_llm_adapter.py::test_google_llm_adapter_returns_response_text -q`로 RED 확인
- [ ] `GoogleGenAiLlmAdapter` 구현
- [ ] `response_mime_type="application/json"` 설정
- [ ] `max_output_tokens`, `temperature`, `timeout_ms` 전달
- [ ] adapter 테스트 GREEN 확인
- [ ] 커밋: `feat: Google Gen AI LLM adapter 추가`

Acceptance:

- unit test는 실제 Google API를 호출하지 않는다.
- provider 실패는 예외 전파가 아니라 `None` 반환이다.

## Sprint 5: Pipeline wiring

### Task 5.1 기본 adapter factory 연결

Files:

- `backend/src/agents/pipeline.py`
- `backend/tests/test_pipeline_edges.py`

Steps:

- [ ] `AgentPipeline()` 기본값이 API 없이 fallback 응답을 반환하는 테스트 추가
- [ ] 기존 placeholder 의존 상태에서 테스트 baseline 확인
- [ ] `AgentPipeline`과 `build_agent_graph()`에서 `create_llm_adapter()` 사용
- [ ] `uv run --extra dev pytest tests/test_pipeline_edges.py -q` 실행
- [ ] 커밋: `feat: LLM adapter factory를 pipeline에 연결`

Acceptance:

- `llm`을 명시 주입하는 기존 테스트는 계속 외부 API를 호출하지 않는다.
- `llm` 미주입 기본 pipeline은 환경변수 기준 adapter를 생성한다.

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
- [ ] `README.md`에 `FACTORY_LLM_PROVIDER=none` 실행 예시 추가
- [ ] `README.md`에 Google Gen AI 실행 예시 추가
- [ ] `rg -n "OpenAI API Configuration|google-generativeai|langchain-google-genai" backend`로 낡은 설정 문구 확인
- [ ] 커밋: `docs: LLM provider 운영 규칙 정리`

## 최종 검증

Steps:

- [ ] `uv run --extra dev pytest`
- [ ] `uv run --extra dev ruff check .`
- [ ] `FACTORY_LLM_PROVIDER=none uv run --extra dev pytest tests/test_pipeline_edges.py -q`
- [ ] `git status --short`
- [ ] `git push -u origin docs/llm-adapter-implementation`

완료 기준:

- 전체 테스트 통과
- Ruff 통과
- 외부 API key 없이 테스트 통과
- sprint별 커밋 분리 완료
