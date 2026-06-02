# Review Feedback - 2026-06-01

## Sprint 6.1 LLM decision routing edge coverage

### Review 1

- Reviewer: Faraday (`reviewer`, gpt-5.5 high)
- Scope:
  - `backend/tests/test_pipeline_edges.py`
  - `backend/docs/plans/llm_implementation_sprint.md`
  - `_workspace/factory-agent/task_6_1_routing_tests_red.md`
- Status: finding resolved by commit step

Finding:

- `backend/docs/plans/llm_implementation_sprint.md`에서 `커밋: test: LLM 기반 agent routing 경로 보강` 항목을 `[x]`로 표시했지만, 리뷰 시점에는 아직 해당 커밋이 없어 사실과 맞지 않았다.

Resolution:

- Sprint 6.1 작업 범위는 커밋까지 포함하므로, 최종 완료 전에 `test: LLM 기반 agent routing 경로 보강` 커밋을 생성해 체크 상태와 실제 git history를 일치시킨다.

Notes:

- 새 테스트들은 실제 LangGraph routing edge 계약을 통과 경로와 JSON 거부 경로 양쪽에서 검증한다.
- `len(llm.prompts)` assertion은 JSON routing decision이 leaf/generation 단계로 진행되지 않았음을 고정한다.
- `backend/tests/test_message_router.py`와 일부 중복이 있지만, Sprint 6.1이 `test_pipeline_edges.py`에 명시적으로 보강하는 계획이므로 허용 가능한 중복으로 판단한다.

## Sprint 8.1 LLM provider 운영 문서

### Review 1

- Reviewer: Newton (`reviewer`, gpt-5.5 high)
- Scope:
  - `backend/README.md`
  - `backend/src/DECISION_LOG.md`
  - `backend/docs/plans/llm_implementation_sprint.md`
  - `_workspace/factory-agent/task_8_1_llm_ops_docs_red.md`
- Status: findings resolved, 재리뷰 필요

Findings:

- `DECISION_LOG.md`의 "routing prompt가 JSON mode를 쓰지 않는다" 설명이 현재 Google adapter의 `response_mime_type="application/json"` 설정과 다르다.
- Sprint 8.1 커밋 항목이 커밋 전 `[x]`로 표시되어 실제 git history와 맞지 않는다.
- 새 결정 로그 항목이 22번인데 문서 중간의 8번 앞에 있어 최신 결정인지 삽입 오류인지 혼동된다.
- 최종 push 명령이 현재 브랜치 `feature/issue-19-agent-routing-terms`가 아니라 stale branch `docs/llm-adapter-implementation`을 가리킨다.

Resolution:

- 결정 로그의 JSON mode 단정 문장을 제거하고, adapter는 raw text를 넘기며 routing/generation 검증은 pipeline이 담당한다고 정리했다.
- Sprint 8.1 커밋 항목은 커밋 전까지 `[ ]`로 되돌렸다.
- 새 결정 로그 항목을 문서 끝의 최신 결정 위치로 이동했다.
- 최종 push 명령을 현재 브랜치 기준으로 수정했다.

### Review 2

- Reviewer: Noether (`reviewer`, gpt-5.5 high)
- Status: 중요 발견 없음

결과:

- 이전 finding 4건은 해소됐다.
- README와 결정 로그 설명은 `LLMSettings`, Google adapter raw text 반환 및 `None` 수렴, LangGraph routing/generation 검증 책임과 충돌하지 않는다.
- 낡은 provider 문구 검색 결과는 runtime 설정 문구가 아니라 sprint 문서의 체크리스트/설명 문맥에서만 나온다.

## FastAPI Agent Connection Manifest

### Review 1

- Reviewer: Epicurus (`reviewer`, gpt-5.5 high)
- Scope:
  - `backend/src/agent_connection/router.py`
  - `backend/src/app.py`
  - `backend/scripts/smoke_agent_pipeline.py`
  - `backend/tests/test_agent_connection_router.py`
  - `backend/tests/test_websocket_endpoint.py`
  - `backend/tests/test_smoke_agent_pipeline_script.py`
  - `backend/README.md`
  - `SESSION_SUMMARY.md`
  - `backend/docs/plans/2026-06-01-fastapi-agent-pipeline-connection-plan.md`
- Status: findings resolved

Findings:

- 계획 문서의 예상 test count가 실제 테스트 수와 달랐다. WebSocket test는 5개, manifest+WebSocket 조합은 7개가 맞다.
- 계획 문서의 `none` smoke 예상 출력에 새 `agent_connection_manifest` 확인이 빠져 있었다.

Resolution:

- 계획 문서의 예상 count를 `5 passed`, `7 passed`로 수정했다.
- `none` smoke 예상 출력에 `PASS none/agent_connection_manifest`를 추가했다.

### Review 2

- Reviewer: Mencius (`reviewer`, gpt-5.5 high)
- Status: 중요 발견 없음

결과:

- 계획 문서의 예상 test count와 smoke output이 구현된 테스트 및 smoke runner와 일치한다.
- FastAPI manifest route와 smoke runner 변경에서 correctness, security, regression finding은 없다.
