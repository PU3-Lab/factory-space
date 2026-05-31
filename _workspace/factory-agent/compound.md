# Compound Mistake Log

이 파일은 작업 중 반복되거나 누적된 실수를 기록하고, 다음 작업에서 같은 문제가 재발하지 않도록 확인 규칙을 남긴다.

## 2026-05-31 Factory Agent Pipeline 작업

### 1. TDD 순서를 지키지 않고 구현을 먼저 진행함

실수:

- 이전 backend agent pipeline 구현은 먼저 production code를 만들고 나중에 테스트를 보강했다.
- 사용자가 "테스트 코드가 하나도 없네", "tdd로 코드 만들었어?"라고 지적한 뒤에야 테스트를 추가했다.

영향:

- RED 상태를 확인하지 못해 테스트가 실제로 결함을 잡는지 증명하지 못했다.
- 구현 의도와 테스트 의도가 뒤늦게 맞춰져 characterization test 성격이 강해졌다.

재발 방지:

- behavior 변경 전 `_workspace/factory-agent/{task_id}_red.md`를 먼저 만든다.
- 실패하는 테스트를 실행하고 실패 이유를 기록하기 전에는 production code를 수정하지 않는다.
- 문서 전용 작업만 failing test 예외로 두고, 이 경우에도 구조 검증 기준을 red artifact에 기록한다.

### 2. Superpowers harness를 먼저 구성하지 않음

실수:

- 프로젝트 시작 시 Superpowers 플러그인과 harness 구조를 먼저 확인하지 않았다.
- 이후 사용자가 "superpowers로 하네스 구성해"라고 지시한 뒤 repo-local harness를 만들었다.

영향:

- 구현, 리뷰, 검증 산출물이 처음부터 같은 규칙으로 남지 않았다.
- 리뷰 전용 artifact와 handoff 파일이 뒤늦게 생성됐다.

재발 방지:

- 프로젝트 시작 또는 새 backend/agent 작업 시작 시 `.agents/skills/`, `docs/harness/`, `_workspace/`를 먼저 확인한다.
- 없으면 최소 harness를 먼저 만든다: team spec, coordinator, implementer, spec reviewer, quality reviewer, handoff 규칙.
- `~/.codex/agents.md`와 `~/.codex/AGENTS.md`에 이 규칙을 추가했다.

### 3. 리뷰 결과를 별도 추적 파일에 바로 남기지 않음

실수:

- 리뷰에서 나온 pipeline 문제를 처음에는 final response로만 전달했다.
- 사용자가 "리뷰시 나온 문제도 다 기록해", "피드백 파일 하나 만들어"라고 지시한 뒤 문서화했다.

영향:

- 피드백이 작업 산출물로 남기 전까지 추적성이 약했다.
- 후속 수정 대상이 commit diff나 대화에 흩어질 수 있었다.

재발 방지:

- 리뷰 결과가 나오면 즉시 `_workspace/factory-agent/review-feedback.md`에 기록한다.
- 결정이 필요한 내용은 `backend/src/DECISION_LOG.md`에도 함께 기록한다.
- review artifact에는 status, severity, file, impact, required fix를 포함한다.

### 4. Agent 계약 문서가 runtime protocol과 어긋남

실수:

- `backend/AGENTS.md`의 Agent 계약 예시가 `build_prompt(self, context)`로 되어 있었다.
- 실제 `Agent` protocol은 `build_prompt(payload, context)`와 `fallback(payload, context)`를 요구한다.

영향:

- 문서를 보고 새 Agent를 만들면 pipeline contract를 만족하지 못할 수 있었다.

재발 방지:

- 계약 문서는 실제 protocol 파일과 같이 확인한다.
- 변경 전후 `backend/src/agents/base.py`와 `backend/AGENTS.md`를 함께 비교한다.
- quality review에서 문서-코드 contract drift를 필수 점검한다.

### 5. Pipeline validation/caching edge 문제를 첫 구현에서 놓침

실수:

- malformed envelope에서 request correlation field가 보존되지 않는 문제를 구현 중 발견하지 못했다.
- cache hit 시 fallback/LLM metadata가 사라지는 문제를 구현 중 발견하지 못했다.

영향:

- 클라이언트가 잘못된 요청의 error response를 request와 매칭하기 어렵다.
- 반복 요청에서 metadata 의미가 달라져 디버깅과 telemetry가 불안정해진다.

재발 방지:

- pipeline 작업 시 error correlation, cache key, cache value, metadata 보존을 별도 테스트 항목으로 둔다.
- cache는 payload만 볼 것이 아니라 payload와 metadata를 함께 검증한다.
- WebSocket 경유 error response도 pipeline 직접 호출과 같은 contract를 만족하는지 확인한다.

### 6. 완료 보고 전에 서브 에이전트 리뷰를 누락함

실수:

- Sprint 4.2 Google Gen AI adapter 구현 후 TDD, 관련 회귀, 전체 테스트, Ruff는 실행했다.
- 하지만 사용자가 정한 "리뷰 전용 에이전트 사용" 규칙에 따라 서브 에이전트 리뷰를 완료하기 전에 작업 완료를 보고했다.

영향:

- 구현 품질 검증 루프가 사용자 합의 절차와 어긋났다.
- 리뷰 결과가 `_workspace/factory-agent/review-feedback.md`에 남기 전에 커밋이 먼저 생성됐다.

재발 방지:

- 기능 구현 커밋 전 또는 완료 보고 전 `reviewer` sub-agent를 반드시 실행한다.
- 서브 에이전트 리뷰가 완료되기 전에는 "완료"라고 말하지 않는다.
- 리뷰에서 나온 finding은 즉시 `_workspace/factory-agent/review-feedback.md`에 기록하고, unresolved finding이 있으면 수정 후 재검증한다.

### 7. 리뷰 finding 수정 후 재리뷰를 누락함

실수:

- Sprint 4.2 Google Gen AI adapter 리뷰에서 나온 finding을 수정하고 검증/커밋했다.
- 하지만 수정 결과를 같은 기준으로 다시 서브 에이전트 리뷰하지 않은 채 완료로 간주했다.

영향:

- 수정이 reviewer의 원래 지적을 실제로 해결했는지 독립 확인이 빠졌다.
- 수정 과정에서 새 regression이나 새 edge case가 생겼는지 확인하는 루프가 닫히지 않았다.

재발 방지:

- reviewer finding이 하나라도 있고 코드를 수정했다면, 수정 후 같은 범위로 `reviewer` sub-agent 재리뷰를 실행한다.
- 재리뷰 결과가 `no unresolved findings`가 될 때까지 수정 -> 검증 -> 재리뷰를 반복한다.
- 최종 보고에는 "리뷰", "수정", "재리뷰" 결과를 구분해서 적는다.

### 8. 함수 내부 import 사용

실수:

- adapter 구현에서 `urllib`, `google.genai` import를 함수 내부에 두었다.
- 사용자가 "함수안에서 import 금지"라고 지적한 뒤에야 규칙을 명시했다.

영향:

- 의존성이 함수 실행 시점에 숨어 import 실패가 늦게 드러난다.
- 모듈 의존성을 파일 상단에서 파악하기 어렵다.
- 테스트나 리뷰에서 의존성 경계를 놓치기 쉽다.

재발 방지:

- import는 항상 파일 상단에 둔다.
- 함수나 메서드 내부 import는 금지한다.
- 새 파일 작성 후 `rg -n "^\\s+import |^\\s+from .* import" <path>`로 내부 import가 없는지 확인한다.
