# 세션 요약 - 2026-05-31

## 현재 상태

- 브랜치: `feature/issue-19-agent-routing-terms`
- 최신 커밋: `ab146be Refs #19 manual_qa를 operator_guide로 변경`
- 현재 미커밋 변경: 이 세션 요약 파일

## 완료한 작업

### Agent 라우팅 용어 정리

- `manual_qa` top-level/domain Agent 명칭을 `operator_guide`로 변경했다.
- 패키지 경로를 `backend/src/agents/manual_qa/`에서 `backend/src/agents/operator_guide/`로 이동했다.
- Domain Orchestrator 클래스는 `OperatorGuideAgent`다.
- top-level Agent id는 `operator_guide`다.
- leaf Agent id는 다음과 같다.
  - `operator_guide.recipe_explainer`
  - `operator_guide.machine_help`
  - `operator_guide.troubleshooter`
- LangGraph node 이름은 `operator_guide.route_sub_agent`를 유지했다. public request payload 필드가 여전히 `sub_agent`이기 때문이다.

### 문서

- backend 계획 문서와 역할 문서의 명칭을 `operator_guide` 기준으로 수정했다.
- 루트 `protocol_plans.md`의 예시와 표시명을 manual Q&A 용어에서 운영자 가이드 용어로 수정했다.
- RED/GREEN artifact를 추가했다: `_workspace/factory-agent/task_operator_guide_rename_red.md`
- 날짜별 리뷰 artifact를 추가했다: `_workspace/factory-agent/review-feedback-2026-05-31.md`

### 프로세스

- production rename 작업 전에 TDD artifact를 먼저 만들었다.
- 리뷰 전용 서브 에이전트 `Dirac`을 `gpt-5.5 high`로 사용했다.
- 리뷰 결과는 날짜별 리뷰 파일에 기록했다.
- 리뷰, 수정, 재리뷰 루프를 `중요 발견 없음`이 나올 때까지 반복했다.

## 검증

- `backend/`에서 `uv run --extra dev ruff check .`: 통과
- `backend/`에서 `uv run --extra dev pytest -q`: 88 passed
- `git diff --check`: 통과
- `protocol_plans.md`, `backend/src`, `backend/tests`, `backend/docs`에서 이전 용어 검색: `manual_qa`, `ManualQa`, `MANUAL_QA`, `Manual Q&A`, `매뉴얼 Q&A`, `route_manual`, `buildManualQaContext` 모두 no matches

## 최근 커밋

- `ab146be Refs #19 manual_qa를 operator_guide로 변경`
- `de4f54a Refs #19 agent routing 용어 정리`
- `0d9e12a feat: LangGraph LLM fallback 경로 연결`
- `083b449 docs: source 파일 500줄 제한 규칙 추가`
- `79c1d7f refactor: LLM 약어 표기 정리`

## 다음 작업 전 확인 규칙

- 작업 시작 전에 `_workspace/factory-agent/compound.md`를 확인한다.
- 동작이나 코드 변경은 production edit 전에 RED artifact를 먼저 만든다.
- 함수 내부 import는 금지한다.
- source 파일은 500줄 이하로 유지하되, 줄 수를 억지로 맞추지 말고 기능 의미 단위로 분리한다.
- Agent 선택은 ad hoc parsing logic이 아니라 prompt/structured-output과 LangGraph conditional routing으로 처리한다.
- 완료 보고나 커밋 전에 reviewer sub-agent를 사용한다.
- 리뷰 결과를 기록하고, 수정 후에는 재리뷰를 반복한다.
- 앞으로 리뷰 파일은 날짜별로 남긴다.
