# operator_guide RAG Sprint 10 Debug Endpoint & Production Wiring Plan

## 목표

Sprint 10에서는 RAG 품질을 LLM 답변 없이 단독 진단하는 디버그 엔드포인트를 제공하고, 실제 웹소켓 서비스에서도 pgvector 데이터베이스 및 Embedding Provider가 정상적으로 로딩되어 동작하도록 런타임 주입(Wiring)을 완성한다.

## 포함 범위

- RAG 검색 품질 진단 전용 API 라우터 신설 (`debug_router.py`)
  - 엔드포인트: `POST /api/v1/debug/manual-rag/search`
- `FACTORY_RAG_DEBUG_ENABLED` 환경변수에 따른 403 Forbidden 제어
- RAG 검색 모니터링 로깅 추가 (`rag_retriever.py`)
  - 원본 질의어, 매칭 수, 스코어, Confidence 정보
- FastAPI `lifespan` 초기화 단계에서 글로벌 RAG runtime 빌드 및 주입 (`app.py`)
- 테스트 환경 격리를 위한 `FACTORY_RAG_RUNTIME_MOCK` 플래그 도입
- 데이터베이스 세션 연동 시 `FACTORY_DATABASE_URL` 환경변수 호환성 보강 (`engine.py`)

## 제외 범위

- LLM 답변 생성 과정 (디버그 API는 순수 RAG 검색 결과와 메타데이터만 반환)
- Unreal UI 화면 변경

## 설계 방향

```text
POST /api/v1/debug/manual-rag/search
-> FACTORY_RAG_DEBUG_ENABLED 검사 (false 시 403 Forbidden)
-> ManualQAService.get_global_rag_runtime() 확인 (런타임 미작동 시 404 Not Found)
-> RAG 검색 질의 수행 및 분석 로깅
-> 결과 JSON 반환 (is_multi_question, 하위 질문별 매칭 정보 등)
```

## 테스트 전략

```text
개발 중:
Sprint 10 디버그 라우터 테스트 단독 실행

구현 완료 직후:
FastAPI lifespan 및 런타임 연동 테스트 실행

수동 검증:
로컬 PostgreSQL pgvector DB 컨테이너와 서버를 띄운 뒤, python 테스트 스크립트를 통해 한글 깨짐 및 응답 성공 확인
```

## 작업 로그

- 2026-06-15: RAG 디버그 API 및 실서버 런타임 연동을 위한 Sprint 10 범위 정의.
- 2026-06-15: 디버그 API 엔드포인트 구현을 위해 `debug_router.py` 추가 및 `app.py` 라우터 등록.
- 2026-06-15: `ManualQAService`에 RAG runtime을 주입할 수 있도록 `set_global_rag_runtime` 클래스 메소드 구현.
- 2026-06-15: FastAPI lifespan 구동 시점에 pgvector 및 Embedding Provider를 빌드하여 `ManualQAService`에 글로벌 주입하도록 `app.py` 보강.
- 2026-06-15: RAG 검색 모니터링 로그 구현 (`rag_retriever.py`).

## 트러블슈팅 로그

- 2026-06-15: FastAPI lifespan 테스트 시 `TestClient`가 실제 lifespan을 가동하여 Mock 런타임을 덮어쓰는 문제가 발생했다. 이를 해결하기 위해 `FACTORY_RAG_RUNTIME_MOCK` 환경변수 플래그를 추가해 테스트 실행 시 실제 런타임 주입을 건너뛰도록 차단했다.
- 2026-06-15: 로컬 구동 테스트 도중 기존 SQLite의 `DATABASE_URL` 우선 참조로 인해 pgvector 문법 에러(`sqlite3.OperationalError`)가 발생했다. `engine.py`에서 `DATABASE_URL`이 미정의되었을 때 `FACTORY_DATABASE_URL`도 fallback으로 활용할 수 있게 보강하여 연동 오류를 해결했다.

## 검증 로그

- 2026-06-15: Sprint 10 단독 테스트 `uv run pytest tests/test_operator_guide_debug_router.py -v` 통과 (3개 케이스).
- 2026-06-15: 백엔드 서버 기동 후 `POST /api/v1/debug/manual-rag/search` API 수동 E2E 테스트 성공 및 로깅 확인.
