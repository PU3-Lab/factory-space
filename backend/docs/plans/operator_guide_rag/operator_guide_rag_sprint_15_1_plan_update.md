# operator_guide RAG Sprint 15.1 보완 계획

## 목적

Sprint 15에서 Current Game State Tool의 기본 흐름은 연결되었지만, master plan과 sprint plan의 최종 기준에는 아직 부족한 부분이 있다.

Sprint 15.1은 Sprint 15를 새 기능으로 확장하기보다, 계획과 구현 사이의 차이를 줄이는 보정 스프린트다.

## 배경

Sprint 15 리뷰에서 다음 보완점이 확인되었다.

- 계획된 state scope 중 `connectedConveyors`, `recentErrorEvents`가 구현과 테스트에 빠져 있다.
- `Context Need Classifier`가 LLM 기반 구조가 아니라 `troubleshooting_question` 여부만 보는 rule-based 구조다.
- 일부 operator_guide 한글 docstring이 깨져 보여 초보자용 설명 문서 기준에 맞지 않는다.

## 포함 범위

- `connectedConveyors` scope 추가
- `recentErrorEvents` scope 추가
- `Context Need Classifier`를 LLM/mockable 구조로 재정리
- 테스트에서는 외부 LLM을 호출하지 않고 mock classifier 또는 fake adapter를 사용
- rule-based fallback은 유지하되, 기본 구조는 provider 교체가 가능한 형태로 둔다.
- 깨진 한글 docstring과 테스트 설명 정리
- Sprint 15 리뷰 문서에 보완 결과 기록

## 제외 범위

- Unreal 실제 상태 API 연동
- 게임 상태를 직접 변경하는 action 실행
- Human-in-the-loop 승인 플로우
- 새로운 RAG 검색 기능 추가
- 대규모 서비스 리팩터링

## 완료 기준

- troubleshooting 질문에서 `requiredStateScopes`에 `connectedConveyors`, `recentErrorEvents`가 포함된다.
- context need 판단 로직이 LLM adapter 또는 mock provider로 교체 가능한 구조가 된다.
- 테스트는 외부 API 없이 context need 판단 결과를 검증한다.
- 일반 질문에서는 Current Game State Tool이 호출되지 않는다.
- 문제 해결 질문에서는 필요한 scope만 필터링되어 prompt context와 metadata에 반영된다.
- operator_guide 실행 코드의 한글 docstring이 깨지지 않고 초보자도 흐름을 이해할 수 있다.

## 검증 계획

```powershell
uv run pytest tests/test_operator_guide_rag_sprint15.py -q
uv run pytest tests/test_operator_guide_rag_sprint15_1.py -q
uv run ruff check
```

커밋 또는 PR 직전에는 operator_guide RAG 관련 테스트 묶음과 전체 테스트를 한 번 더 확인한다.

## 작업 로그

- 2026-06-16: Sprint 15 리뷰 결과를 반영해 Sprint 15.1 보완 계획을 추가했다.

## 트러블슈팅 로그

- 2026-06-16: Sprint 15 구현은 mock 기반 current state 연결까지는 완료되었으나, 계획된 state scope 전체와 LLM/mockable classifier 구조가 빠져 있어 보완 스프린트로 분리했다.
