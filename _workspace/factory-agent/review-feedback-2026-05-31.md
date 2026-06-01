# Review Feedback - 2026-05-31

## Operator Guide rename

Status: complete

Scope:

- `manual_qa` top-level/domain Agent id와 package name을 `operator_guide`로 rename한다.
- 날짜별 리뷰 기록 규칙에 따라 이번 작업 리뷰 결과는 이 파일에 남긴다.

Review 1:

- Reviewer: Dirac (`reviewer`, gpt-5.5 high)
- Result: 중요 발견 없음.
- 확인: `backend/src`, `backend/tests`, `backend/docs`에는 `agent_id = "manual_qa"`, `manual_qa.*`, `ManualQa`, `MANUAL_QA`, `route_manual_*` 잔재가 없다.
- 확인: LangGraph routing 계약은 `operator_guide` top-level, `operator_guide.recipe_explainer|machine_help|troubleshooter` leaf, `operator_guide.route_sub_agent` node 이름으로 유지된다.
- 남은 리스크: 범위 밖 root 문서 `protocol_plans.md`에 `buildManualQaContext()`, `"agent": "manual_qa"`, `Manual Q&A Agent`, `manual_qa` module 예시가 남아 있었다.
- 남은 리스크: 이 리뷰 기록 파일의 status가 `in progress`였고 실제 리뷰 결과가 비어 있었다.

Fix after review 1:

- `protocol_plans.md`의 예전 manual Q&A 예시를 `operator_guide` 명칭으로 정리했다.
- 이 날짜별 리뷰 기록 파일에 리뷰 결과와 후속 수정 내역을 기록했다.

Review 2:

- Reviewer: Dirac (`reviewer`, gpt-5.5 high)
- Finding: P3 문서 용어 잔재. `protocol_plans.md`에 `매뉴얼 Q&A Agent` / `매뉴얼 Q&A` 표시명이 남아 있었다.

Fix after review 2:

- `protocol_plans.md`의 `매뉴얼 Q&A Agent` / `매뉴얼 Q&A` 표시명을 `운영자 가이드 Agent` / `운영자 가이드`로 정리했다.

Review 3:

- Reviewer: Dirac (`reviewer`, gpt-5.5 high)
- Result: 중요 발견 없음.
- 확인: `protocol_plans.md`, `backend/src`, `backend/tests`, `backend/docs`에서 이전 용어 패턴은 더 이상 잡히지 않는다.
- 확인: LangGraph 계약은 `operator_guide` top-level, `operator_guide.recipe_explainer|machine_help|troubleshooter` leaf, `operator_guide.route_sub_agent` node 이름으로 유지된다.

## Agent pipeline smoke runner

Status: complete

Scope:

- `none`, `local`, `providers` 순서의 Agent pipeline smoke runner를 추가한다.
- smoke 가능한 단계면 smoke test 또는 smoke script를 추가한다는 규칙을 문서화한다.

Review 1:

- Reviewer: Newton (`reviewer`, gpt-5.5 high)
- Result: 중요 발견 없음.
- 확인: `all` 실행 순서는 `none` -> `local` -> `providers`다.
- 확인: `providers` profile은 `FACTORY_SMOKE_EXTERNAL_PROVIDER=1` opt-in이 없으면 서버 접속 전에 skip한다.
- 확인: `none` profile은 health, invalid JSON, invalid envelope, routing unavailable만 포함한다.
- 확인: `local` / `providers` profile은 네 top-level Agent 경로를 모두 포함한다.

Review 2:

- Reviewer: Newton (`reviewer`, gpt-5.5 high)
- Finding: README의 `none` smoke 예시가 `.env.example`을 그대로 복사하면 local LLM 기본값 때문에 `ROUTING_UNAVAILABLE` 기대와 어긋날 수 있다.

Fix after review 2:

- `backend/.env.smoke-none.example`를 추가해 default/fallback1/fallback2 provider를 모두 `none`으로 둔 smoke 전용 env 예시를 만들었다.
- README와 sprint/session 문서를 smoke-none env 기준으로 갱신했다.

Review 3:

- Reviewer: Newton (`reviewer`, gpt-5.5 high)
- Finding: `backend/.env.smoke-none.example`는 `.gitignore`의 `.env.*` 패턴에 걸려 커밋에 포함되지 않는다.

Fix after review 3:

- ignored 되는 `.env.smoke-none.example` 대신 추적 가능한 `backend/smoke-none.env.example`로 변경했다.
- README, sprint 문서, session summary, task artifact를 `smoke-none.env.example` 기준으로 갱신했다.

Review 4:

- Reviewer: Newton (`reviewer`, gpt-5.5 high)
- Result: 중요 발견 없음.
- 확인: `backend/smoke-none.env.example`는 ignored가 아니라 추적 가능한 파일이다.
- 확인: README의 `none` smoke 명령은 `uv run --env-file smoke-none.env.example ...` 흐름으로 smoke 전제와 일치한다.
