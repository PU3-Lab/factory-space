# operator_guide 명시적 Agent 라우팅 보정 계획

## 문제

Unreal 또는 agent-test가 요청 JSON에 `"agent": "operator_guide"`를 명시해도 공통 pipeline이 상위 Orchestrator LLM을 다시 호출한다.

라우팅 LLM이 응답하지 않으면 operator_guide의 prompt injection guardrail까지 도달하지 못하고 `ROUTING_UNAVAILABLE` 오류가 반환된다.

## 원인

`route_top_agent` 노드가 요청에 포함된 유효한 Agent ID를 힌트로만 사용하고, 항상 LLM 라우팅 결과를 요구한다.

## 수정 범위

1. 요청의 `agent`가 `operator_guide`이면 해당 Agent로 직접 라우팅한다.
2. `agent`가 없거나 유효하지 않으면 기존 Orchestrator LLM 라우팅을 유지한다.
3. 가드레일 시연용 프리셋과 실제 leaf agent 동작을 시연 문서에 설명한다.

## 검증

1. 유효한 명시적 Agent가 상위 라우팅 LLM 없이 실행되는 회귀 테스트
2. 기존 prompt 기반 라우팅 및 오류 처리 테스트
3. operator_guide prompt injection guardrail 테스트
4. Ruff 검사

## 작업 로그

- 2026-06-22: `ROUTING_UNAVAILABLE` 재현 결과, 가드레일 이전의 상위 Agent 라우팅 실패가 원인임을 확인했다.
- 2026-06-22: 명시적 `operator_guide` 요청은 상위 Orchestrator LLM을 건너뛰도록 보정했다.
- 2026-06-22: agent-test 가드레일 시연은 `설비 도움말` 프리셋과 `operator_guide.machine_help` leaf를 사용하도록 문서화했다.
