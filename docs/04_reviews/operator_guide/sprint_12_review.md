# 코드 리뷰: operator_guide RAG Sprint 12 (Memory Evaluation & Runtime Tuning)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-runtime-docs` |
| 리뷰 일자 | 2026-06-16 |
| 리뷰 범위 | 대화 기억 기능 (`session_memory.py`), RAG 검색 쿼리 확장 및 연동 (`service.py`, `manual_context_builder.py`), 파이프라인 컨텍스트 업데이트 (`runtime.py`), 테스트 및 버그 수정 |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- **확정 사실(Confirmed Facts) 추출 및 병합 구현**: 플레이어의 질문 텍스트에서 게임 상태 사실을 파싱하는 `extract_confirmed_facts` 규칙 및 `OperatorGuideSessionMemory` 내 세션 누적 저장 구조를 마련했습니다. 전력 정상/부족과 같이 상충되는 상태가 발생하면 기존 상태를 제거하고 업데이트하며 `summary_version`을 1씩 자동 증가시킵니다.
- **RAG 검색 쿼리 확장(Query Expansion)**: 플레이어가 "그럼 다음은?" 같은 이전 맥락 없이는 모호한 후속 질문을 했을 때, 세션에 저장된 `confirmed_facts`들을 합친 확장된 검색 쿼리를 RAG 검색기(`MultiQuestionRagRetriever`)에 전달해 매뉴얼 매칭 정확도를 고도화하였습니다.
- **파이프라인 메타데이터 및 하위 호환성 연동**: 응답 메타데이터의 `memory` 내에 `confirmed_facts` / `confirmedFacts` 및 `summary_version` / `summaryVersion` 형태로 필드를 탑재하여 클라이언트와의 API 규격을 충족시켰습니다.
- **로컬 빌드 및 환경성 버그 해결**: Windows 환경에서 SQLite 임시 파일 삭제 시 발생하는 프로세스 락 오류를 `engine.dispose()`를 통해 제거하고, 누락된 `manual_qa_docs_router`를 `app.py`에 등록하여 404 테스트 실패 현상을 조치했습니다.

---

## 2. 이슈 목록

심각도: 🔴 Blocker · 🟠 Major · 🟡 Minor · ⚪ Nit

### 🟡 m1. 텍스트 프롬프트와 메시지 프롬프트 빌드 과정의 RAG 중복 조회 발생
- **위치**: `backend/src/agents/pipeline/runtime.py` -> `build_prompt` 노드
- **내용**: `build_prompt` 노드 수행 시 `agent.build_prompt`와 `agent.build_prompt_messages`가 각각 `build_prompt_context`를 호출함에 따라, 동일한 턴 내에서 RAG 검색(`self._rag_runtime.retrieve`)이 2회 중복 실행되고 있습니다.
- **영향**: LLM 호출 전 RAG retrieval 레이턴시 및 임베딩 API 호출 비용이 배로 증가할 우려가 있습니다.
- **제안**: 현재 스프린트는 기능 검증 및 메모리 평가가 주 목적이므로 3회 턴(총 6회 쿼리) 검증에 성공했으나, 향후 런타임 성능 최적화 스프린트(Tuning) 단계에서 동일 요청 내 RAG 검색 결과를 메모이징하거나 캐싱하는 구조로 개선할 것을 권고합니다.

### 🟡 m2. camelCase와 snake_case 메타데이터 키 이중 지원
- **위치**: `backend/src/agents/pipeline/runtime.py` -> `operator_guide_memory_context`
- **내용**: 기획안에 기술된 camelCase(`confirmedFacts`, `summaryVersion`)와 기존 코드의 snake_case 스타일(`confirmed_facts`, `summary_version`)을 안전하게 호환하기 위해 메타데이터 딕셔너리에 두 버전을 이중 기재했습니다.
- **영향**: 호환성은 우수하나 데이터 중복이 존재합니다.
- **제안**: 프론트엔드/Unreal 클라이언트의 API 계약 명세가 단일 케이스로 확정되는 스프린트 마무리 단계에서 단일 포맷으로 정리(Refactoring)할 것을 권고합니다.

---

## 3. 우선순위 권고

1. **m1 (RAG 중복 조회 최적화)** - 실제 상용 서비스 배포 전, 턴당 RAG 호출 횟수를 1회로 보장할 수 있는 prompt context 공유 방안을 마련해야 합니다.
2. **m2 (메타데이터 명세 단일화)** - 클라이언트 파트와 협의하여 메타데이터 포맷을 camelCase 또는 snake_case 하나로 통일하고 정리합니다.

---

## 4. 긍정적인 부분

- **대화 문맥 유지 및 RAG 정확도 향상**: "그럼 다음은?", "전력은 정상인데?"와 같이 질문 단독으로는 pgvector 매칭이 불가능한 쿼리가, 이전 질문에서 확인된 사실에 의해 `"그럼 다음은? 컨베이어가 멈춤 전력 상태는 정상"`으로 확장되어 정상 매뉴얼을 찾아내는 획기적인 품질 개선이 입증되었습니다.
- **단위 및 통합 시뮬레이션 테스트 도입**: 멀티턴 세션 시나리오를 Stub RAG 런타임을 활용해 검증하는 통합 테스트(`test_operator_guide_sprint12_evaluation.py`)를 함께 배포함으로써 RAG 품질을 쉽게 리그레션 없이 점검할 수 있게 되었습니다.
- **로컬 테스트 빌드 안정성**: Windows 락 및 404 오류 제거를 통해 로컬 `pytest`가 100% 그린 빌드를 통과하고 있습니다.

---

## 5. 추가 검증 로그

- `uv run pytest tests/test_operator_guide_sprint12_evaluation.py -v` 통과.
- `uv run pytest tests/test_operator_guide_session_memory.py -v` 통과.
- `uv run pytest tests/test_manual_qa_docs_router.py tests/test_migrations_smoke.py -v` 통과.
- `uv run pytest -q` (전체 240개 테스트) 통과.
- `uv run ruff check` 통과.
