# 코드 리뷰: operator_guide RAG Sprint 10 (Debug Endpoint & Production Wiring)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-sprint10` |
| 리뷰 일자 | 2026-06-15 |
| 리뷰 범위 | RAG 디버그 API 라우터(`debug_router.py`), FastAPI `lifespan` 런타임 Wiring (`app.py`), RAG 검색 모니터링 로깅 (`rag_retriever.py`), 데이터베이스 세션 환경 변수 호환성 보강 (`engine.py`) |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- RAG 검색 품질을 LLM 답변 과정 없이 단독 진단하는 디버그 엔드포인트 `POST /api/v1/debug/manual-rag/search` API 라우터 추가.
- API 진입부에서 `FACTORY_RAG_DEBUG_ENABLED` 환경변수가 `true`로 설정되지 않았을 경우 `403 Forbidden`을 리턴하는 보안 제어 구성.
- RAG 검색 시 쿼리, 매칭 수, 스코어, Confidence 정보 등을 기록하는 시스템 표준 로깅 심기 (`rag_retriever.py`).
- FastAPI `lifespan` 초기화 및 종료 주기에 맞춰 pgvector 데이터베이스 및 Embedding Provider를 빌드해 `ManualQAService`에 글로벌 주입(Wiring)하도록 설계.
- 테스트 환경 격리를 보장하기 위한 `FACTORY_RAG_RUNTIME_MOCK` 환경변수 플래그 도입 및 우회 처리 반영.
- DB 연결 호환성 개선을 위해 `engine.py`에서 `DATABASE_URL`이 없을 때 `FACTORY_DATABASE_URL`을 fallback으로 적용하도록 보강.

전반적으로 RAG 런타임을 웹소켓 실서비스에 연동하고, 검색 품질 평가를 쉽게 할 수 있는 모니터링 API와 로깅 인프라가 깔끔하게 구현되었다. 아래는 머지 전 발견 및 조치된 이슈 목록이다.

## 2. 이슈 목록

심각도: 🔴 Blocker · 🟠 Major · 🟡 Minor · ⚪ Nit

### 🔴 B1. `engine.py`에서 `DATABASE_URL` 우선 적용으로 인한 SQLite pgvector 문법 에러

- 위치: `backend/src/db/engine.py:13-22`
- 내용: `engine.py`가 RAG 런타임 바인딩 시 `DATABASE_URL` 환경 변수만 바라보고 있어, `.env.prod`에 기재된 `FACTORY_DATABASE_URL`이 누락된 상태에서 SQLite(`sqlite:///./factory_space.db`)를 강제 로드했다. SQLite 환경에서 pgvector 전용 vector distance 정렬 연산자를 호출함에 따라 `sqlite3.OperationalError: near ">": syntax error`가 발생하며 RAG 서비스 전체가 불능이 됨.
- 영향: 로컬 구동 및 실서버 테스트 시 RAG 검색 기능 동작 실패.
- 제안: `DATABASE_URL`이 정의되지 않았을 때 프로젝트 전반에서 쓰는 `FACTORY_DATABASE_URL`을 fallback으로 함께 확인하도록 수정.

> **[조치 완료 - 2026-06-15]** `get_database_url` 함수에서 `os.environ.get("DATABASE_URL") or os.environ.get("FACTORY_DATABASE_URL")` 형태로 환경 변수를 병합 조회하도록 보강하여 pgvector 에러를 깔끔하게 해소했습니다.

### 🟠 M1. FastAPI lifespan 테스트 시 Mock 런타임 오버라이드 문제

- 위치: `backend/src/app.py:33-49`
- 내용: 유닛 테스트 실행 시 `TestClient`가 FastAPI lifespan을 시작하면서 실제 pgvector DB와 OpenAI Embedding 서비스를 엮은 실시간 런타임을 글로벌 주입했다. 이로 인해 테스트 코드 내에서 격리 검증을 위해 셋업했던 Mock RAG 런타임이 오버라이드되는 현상이 발생했다.
- 영향: 유닛 테스트 환경 오염 및 RAG 런타임 Mocking 테스트 실패 유발.
- 제안: 테스트 세션과 같이 RAG 런타임을 Mocking해야 하는 경우 강제로 lifespan 초기화를 우회하는 별도 환경변수 플래그 도입.

> **[조치 완료 - 2026-06-15]** `FACTORY_RAG_RUNTIME_MOCK` 환경 변수가 `true`인 경우 lifespan 내 RAG 초기화 로직을 우회하도록 예외 분기 처리를 반영했습니다.

---

## 3. 우선순위 권고

1. **B1** - SQLite pgvector 에러는 RAG 프로덕션 가동 자체를 불가능하게 하므로 즉각 수정되어야 함 (수정 완료).
2. **M1** - 테스트 격리 오염을 유발하므로 즉각 반영되어야 함 (수정 완료).

## 4. 긍정적인 부분

- RAG 품질만을 독립적으로 검사할 수 있는 디버그 엔드포인트를 구현하여 개발 편의성 및 품질 모니터링 능력이 대폭 향상됨.
- RAG 초기화 실패 시 예외 처리를 거쳐 기존 CSV-only 모드로 graceful fallback하는 안정적인 결함 격리 설계가 반영됨.
- 질문 분해기(Decomposer)를 거친 다중 질문 검색 구조와 confidence 산출이 자연스럽게 백엔드 런타임에 통합됨.
