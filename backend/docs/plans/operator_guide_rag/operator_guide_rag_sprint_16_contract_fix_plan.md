# Sprint 16.1 Unreal Contract 보정 계획

## 목적

Sprint 16에서 작성한 Unreal 연동 계약 문서가 실제 WebSocket 프로토콜과 완전히 일치하도록 보정한다.

현재 백엔드는 `AgentRequestEnvelope` / `AgentResponseEnvelope` 구조를 사용한다. 따라서 Unreal 문서도 질문을 최상위 필드가 아니라 `payload.question`으로 보내고, 응답 metadata도 최상위가 아니라 `payload.metadata` 안에서 읽도록 설명해야 한다.

## 수정 범위

- `docs/04_reviews/operator_guide/unreal_contract.md`
  - 요청 JSON을 실제 `agent.request` envelope 구조로 수정
  - 응답 JSON을 실제 `agent.response` envelope 구조로 수정
  - 모든 예시 시나리오의 metadata 위치를 `payload.metadata`로 통일
  - 현재 상태가 필요한 질문은 `context.current_game_state`로 전달하는 예시 추가

- `docs/04_reviews/operator_guide/sprint_16_review.md`
  - 현재 브랜치명으로 정리
  - 계약 문서가 실제 envelope 구조로 보정되었음을 리뷰 결과에 반영

## 완료 기준

- Unreal 팀이 문서의 요청 JSON을 그대로 WebSocket `/ws/agent`에 보낼 수 있다.
- 응답 JSON에서 Unreal이 읽어야 하는 필드 위치가 명확하다.
- `metadata`, `sources`, `confidence`, `recommended_actions`, `retrieval`, `context` 정보가 모두 `payload.metadata` 기준으로 설명된다.
- 문서 변경만 있으므로 코드 테스트는 실행하지 않고, diff 검증으로 마무리한다.

## 작업 로그

- 2026-06-16: Sprint 16 리뷰에서 요청/응답 JSON 예시가 실제 백엔드 envelope와 다르다는 문제를 확인했다.

## 트러블슈팅 로그

- 2026-06-16: 기존 문서가 개념 설명용 JSON에 가까워 Unreal에서 그대로 사용하면 `payload.question` 누락과 metadata 위치 오해가 발생할 수 있어 실제 프로토콜 기준으로 보정하기로 했다.
