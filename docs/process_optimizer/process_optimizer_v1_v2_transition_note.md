# Process Optimizer v1/v2 전환 기록

## 현재 상태

`process_optimizer`는 처음에 v1 MVP로 구현되었다. v1은 `ProcessOptimizerAgent` 클래스가 LLM prompt와 deterministic fallback을 통해 `suggestion` 형태의 분석/제안 응답을 만드는 구조였다.

현재 public pipeline 경로는 v2 LangGraph로 전환되었다.

```text
agent: "process_optimizer"
-> validate_process_payload
-> process_optimizer_v2_graph
-> state_update / analyze / apply / undo / measure
```

따라서 Unreal WebSocket이나 `/agent-test`에서 `agent: "process_optimizer"`로 들어오는 주요 요청은 v1 LLM prompt 경로가 아니라 v2 graph를 탄다.

## v1의 역할

v1 코드는 삭제하지 않고 `archive_reference`로 유지한다.

```text
backend/src/agents/process_optimizer/agent.py
```

남기는 이유는 다음과 같다.

- v1 MVP 동작을 직접 테스트하는 기존 단위 테스트가 있다.
- v2 preview 설명 문구나 fallback 정책을 비교할 기준으로 쓸 수 있다.
- v2 운영 중 문제가 생겼을 때 초기 deterministic suggestion 구조를 참고할 수 있다.

단, v1은 더 이상 public `process_optimizer` 요청의 메인 실행 경로가 아니다.
또한 기본 agent router에 등록하지 않으므로 WebSocket/agent-test의 `analyze`, `apply`, `undo`, `measure` 요청은 v1을 거치지 않는다.

## v2의 역할

v2는 전용 LangGraph 기반 구현이다.

```text
backend/src/agents/process_optimizer/graph.py
backend/src/agents/process_optimizer/nodes.py
backend/src/agents/process_optimizer/preview_store.py
backend/src/agents/process_optimizer/commands.py
backend/src/agents/process_optimizer/execution_record.py
backend/src/agents/process_optimizer/undo.py
backend/src/agents/process_optimizer/effect_measurement.py
```

v2가 담당하는 흐름은 다음과 같다.

```text
analyze -> preview plan 생성
apply -> 승인/리비전/선택 변경 검증 후 Unreal command payload 생성
undo -> 실행 기록과 현재 상태 비교 후 충돌 검증
measure -> 적용 전후 지표 비교와 결과 분류
```

## 전환 완료 기준

현재 완료된 항목:

- v2 Sprint 1~8 구현
- state_update/analyze/apply/undo/measure graph 흐름 구현
- public pipeline 라우팅을 v2 graph로 전환
- v1은 archive/reference 위치로 명확화

남은 정리 후보:

- v1 prompt 테스트명을 `legacy_v1` 기준으로 변경
- v1 prompt 내부 깨진 주석/문자열 정리
- v2가 충분히 안정화되면 v1 클래스를 archive하거나 문서용 reference로 축소

## 포트폴리오 설명 문장

Process Optimizer는 초기 v1 MVP에서 LLM 기반 제안 응답을 먼저 구현한 뒤, 실제 공장 변경 승인과 되돌리기, 효과 측정까지 다루기 위해 v2 전용 LangGraph 구조로 전환했다. 현재 public 요청은 v2 graph를 사용하며, v1은 archive/reference로 보존해 비교와 회귀 검증 기준으로 활용한다.
