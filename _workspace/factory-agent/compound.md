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

### 9. source 파일 500줄 초과 방치

실수:

- `backend/src/agents/pipeline.py`가 700줄을 넘었는데도 기능을 계속 추가했다.
- 사용자가 "코드 500줄 넘기지 말라는 규칙이 없던가?"라고 지적한 뒤에야 명시 규칙을 추가했다.

영향:

- pipeline의 validation, routing, cache, LLM fallback, response assembly 책임이 한 파일에 과도하게 모인다.
- 리뷰와 테스트 실패 원인 추적 비용이 커진다.
- 후속 작업자가 변경 범위를 과대하게 잡기 쉽다.

재발 방지:

- 일반 source 파일은 500줄을 넘기지 않는다.
- 500줄을 넘기면 역할 기준으로 파일을 분리한다.
- 작업 전후 `wc -l`로 주요 source 파일 길이를 확인한다.

### 10. Agent routing parser naming이 책임 경계를 흐림

실수:

- `parse_agent_selection`, `parse_sub_agent_selection`, `selected` 같은 이름을 남겨 code가 Agent를 선택하는 것처럼 보이게 했다.
- reviewer는 기능 동작은 확인했지만 사용자가 반복해서 강조한 "Agent 선택은 prompt, 구분은 LangGraph conditional edge" naming/책임 경계까지 잡지 못했다.

영향:

- prompt 기반 decision 검증 코드와 실제 분기 로직의 책임이 혼동된다.
- 이후 작업자가 `route_top_agent` 안에 selection shortcut이나 allowlist 분기를 다시 넣기 쉬워진다.

재발 방지:

- LLM 출력 파싱 함수는 `selection`이 아니라 `route_decision`처럼 검증 역할이 드러나는 이름을 쓴다.
- `route_*` node는 state 기록까지만 하고, 경로 구분은 LangGraph conditional edge에서 한다.
- 리뷰 요청에는 동작뿐 아니라 naming이 아키텍처 책임을 흐리는지도 명시적으로 포함한다.

### 11. Sub-agent routing parser를 제거하지 않고 남김

실수:

- top-level routing은 structured prompt와 LangGraph conditional edge로 바꿨지만, `manual_qa`와 `quest_generator`에는 `parse_sub_agent_route_decision()` JSON parser가 남아 있었다.
- 이 parser는 `{"sub_agent": "...", "reason": "..."}` compact JSON을 허용하고 plain id 문자열을 거부해 최신 routing 계약과 반대로 동작했다.

영향:

- `Agent 선택은 prompt, 구분은 LangGraph conditional edge`라는 사용자 지침과 코드가 다시 어긋났다.
- top-level과 sub-agent routing 계약이 달라져 테스트, 문서, 구현을 함께 읽어도 실제 동작을 오해하기 쉬웠다.

재발 방지:

- routing prompt는 top-level과 sub-agent 모두 허용 id 문자열 하나만 반환하게 한다.
- routing node는 모델 raw output을 trim해서 state에 기록하는 일만 한다.
- 허용 id 검증과 경로 분기는 LangGraph conditional edge에만 둔다.
- compact JSON parser는 leaf generation response parsing에만 사용하고, routing에는 만들지 않는다.

### 12. 실행 대상 Agent를 `selectedSubAgent`로 부름

실수:

- `process_optimizer`, `new_material_generator`처럼 하위 Agent가 없는 leaf top-level Agent까지 `selectedSubAgent` state/metadata로 표현했다.
- 공통 conditional edge 이름도 `route_sub_agent_result`라서 domain sub-agent 전용 검증처럼 보였다.

영향:

- top-level Agent, Domain Orchestrator, Leaf Agent의 계층이 문서와 코드에서 다시 혼동됐다.
- 사용자가 "용어 헛갈리지 않게 정확히 명칭해"라고 지적할 만큼 state 이름이 실제 책임을 가리지 못했다.

재발 방지:

- 실제 실행 대상은 항상 `selectedLeafAgent`로 부른다.
- top-level routing 결과는 `selectedAgent`, 실행 leaf 결과는 `selectedLeafAgent`로 구분한다.
- public request payload의 `sub_agent`는 입력 힌트 이름으로만 유지한다.
- 공통 leaf 검증 edge는 `route_selected_leaf_agent`처럼 역할을 드러내는 이름을 쓴다.

### 13. 사용자-facing 문서를 한국어로 작성하라는 흐름을 놓침

실수:

- 사용자가 이전부터 "한글로", "PR 제목과 본문은 한국어", "prompt 한글로 작성"처럼 문서/응답 언어 기준을 반복해서 지시했다.
- 그런데 루트 `SESSION_SUMMARY.md`를 처음 만들 때 영어 템플릿 형태로 작성했다.
- 사용자가 "한글로"라고 다시 지적한 뒤에야 한국어로 고쳤다.

영향:

- 사용자-facing 산출물이 프로젝트의 언어 규칙과 맞지 않았다.
- 이미 합의된 작업 방식이 세션 전환/요약 작업에서 이어지지 않았다.
- 사용자가 같은 지침을 반복해서 확인해야 했다.

재발 방지:

- 사용자-facing 문서, PR 제목/본문, prompt, 세션 요약은 기본값을 한국어로 둔다.
- 새 문서를 만들기 전 기존 AGENTS.md, 프로젝트 문서, 최근 사용자 지시의 언어 규칙을 확인한다.
- 영어 식별자, 코드 심볼, 명령어는 그대로 두되 설명 문장은 한국어로 작성한다.
- 작성 후 `sed -n '1,80p' <문서>`로 첫 화면을 확인해 제목과 본문 언어가 맞는지 점검한다.

### 14. Smoke test 추가 시점을 규칙으로 고정하지 않음

실수:

- Agent pipeline이 WebSocket endpoint와 LangGraph 실행 경로까지 갖춘 뒤에도 smoke test 추가 시점을 명시 규칙으로 두지 않았다.
- unit/edge test만으로 충분한지, 실제 서버 경유 smoke를 언제 추가해야 하는지 판단 기준이 문서화되지 않았다.

영향:

- 실제 실행 경로가 준비되어도 smoke test가 후순위로 밀릴 수 있다.
- provider, local LLM, WebSocket transport처럼 unit test로만 잡기 어려운 연결 문제가 늦게 발견될 수 있다.

재발 방지:

- 서버, pipeline, WebSocket, provider 연결처럼 실제 실행 경로가 생기면 smoke test 또는 smoke script를 함께 추가한다.
- 외부 API나 local service가 필요한 smoke는 기본 test suite에 넣지 않고 opt-in profile로 분리한다.
- API key 없이 가능한 smoke는 기본 profile로 만들고, provider smoke는 명시 환경 변수로만 실행한다.
- smoke 추가가 가능한지 작업 계획 단계에서 먼저 판단하고, 가능하면 acceptance에 포함한다.
