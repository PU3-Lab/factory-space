# LLM 구현 계획 RED 기록

## 작업 유형

문서 전용 작업이다.

## 테스트 예외 사유

이번 작업은 런타임 behavior를 변경하지 않는다. 따라서 failing test를 먼저 작성하지 않는다.

대신 다음 문서 검증 기준을 사용한다.

- 현재 `backend/src/llm/adapter.py`의 placeholder 상태를 반영한다.
- 현재 `backend/pyproject.toml`의 LLM 관련 의존성을 반영한다.
- 기존 결정 로그의 prompt 기반 routing 원칙을 위반하지 않는다.
- LLM 장애 시 fallback으로 복구한다는 pipeline 원칙을 유지한다.
