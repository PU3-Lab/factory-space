# operator_guide RAG Sprint 16 기획서 (End-to-End Unreal Contract & Portfolio Polish)

## 1. 개요
Sprint 16은 기존 구현한 operator_guide RAG 에이전트의 실시간 게임 상태 연동 및 챗봇 엔진의 규격을 최종 마무리하는 단계입니다.
실제 코드 구현보다는 클라이언트(Unreal Engine)와의 **최종 연동 규격(계약 및 스키마)**을 마감하고, 다양한 시나리오별 시연 세트를 구체화하며, RAG 시스템의 장점과 아키텍처를 소개하는 **포트폴리오 설명서**를 구축하는 데 중점을 둡니다.

## 2. 목표 및 범위

### 2.1. Unreal 연동용 계약 문서 정리
- **WebSocket 요청/응답 JSON 규격 정의**:
  - 클라이언트가 보내는 질문 포맷 및 메타데이터 필드.
  - 에이전트가 리턴하는 JSON 응답 구조 (`final_answer`, `recommended_actions`, RAG `sources`, `confidence`, `metadata` 등).
  - 실시간 게임 상태 트래킹을 위한 키 (`requiresCurrentGameState`, `usedCurrentGameState`, `requiredStateScopes`, `availableScopes`)의 카멜케이스 속성 정의.
- **Current Game State 입력 스키마 및 가이드**:
  - `current_game_state` 딕셔너리에 들어올 수 있는 7대 스코프 상세 자료형 명세.
- **질문 가이드 탭 action 계약**:
  - 추천 액션 (`recommended_actions`)의 버튼 연동 라벨 및 고유 액션 ID 목록 정의.

### 2.2. 시연 및 검증 시나리오 정리 (agent-test)
- 대표 시연 질문 세트 6종 정의:
  1. **장비 설명** (예: "제련기는 뭐야?") -> RAG 전용, 상태 도구 미호출
  2. **제작법 설명** (예: "기어는 어떻게 만들어?") -> RAG 전용, 상태 도구 미호출
  3. **단순 트러블슈팅** (상태 미제공) -> 규칙 기반 혹은 LLM에 의해 상태 도구 호출 필요 판단(`requiresCurrentGameState=True`), 제공된 상태가 없어 RAG 단독 또는 fallback 응답.
  4. **실시간 정황 연계 트러블슈팅** (상태 제공) -> 상태 도구 호출 및 성공 연동(`usedCurrentGameState=True`), RAG + 상태 조합 응답.
  5. **범위 밖(out-of-scope) 질문** (예: "오늘 날씨 어때?") -> fallback 및 unknown_question 의도 분류.
  6. **멀티 쿼리(multi-question) 질문** (예: "철광석은 어디서 구하고, 철괴는 어떻게 만들어?") -> RAG 멀티 검색 작동 검증.

### 2.3. 포트폴리오 문서 정리
- **아키텍처 데이터 흐름 기술**:
  - `플레이어 질문 -> Orchestrator -> operator_guide 에이전트 -> ContextNeedClassifier 판단 -> CurrentGameStateTool 정제 -> RAG Store 검색 -> LLM Answer -> Unreal JSON 응답` 파이프라인.
- **실무형 RAG 에이전트 특징 정리**:
  - content_hash 기반 똑똑한 Ingestion 파이프라인.
  - 부분 실패 격리 및 재시도 기능 (Partial Failure & Retry).
  - 고유 버저닝 연동 (`source_version` 활용).
  - 에러 상황을 우회하는 Fallback 다중화 설계.

---

## 3. 완료 기준
- Unreal 연동용 최종 스키마 계약서(`unreal_contract.md`) 정리 완료.
- 6대 시연 및 테스트 시나리오 정리 완료.
- 아키텍처 및 RAG 에이전트 이점이 담긴 포트폴리오 설명서(`portfolio_guide.md`) 정리 완료.
- Sprint 16 통합 검증 문서 및 리뷰 문서 작성 완료.
