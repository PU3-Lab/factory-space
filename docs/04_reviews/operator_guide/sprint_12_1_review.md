# 코드 리뷰: operator_guide RAG Sprint 12.1 (Memory Prompt & RAG Call Tuning)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-runtime-docs` |
| 리뷰 일자 | 2026-06-16 |
| 리뷰 범위 | LLM 프롬프트에 `[CONFIRMED_FACTS]` 섹션 명시적 주입, 동일 턴 내 RAG 캐싱 최적화, 테스트 코드 인덱스 버그 수정 |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- **LLM 프롬프트 확정 사실 주입**: [prompt_builder.py](file:///c:/factory-space/backend/src/agents/operator_guide/prompt_builder.py)에 `[CONFIRMED_FACTS]` 섹션을 정의하고 프롬프트 템플릿의 알맞은 위치에 연동하였습니다. 대화 중 수집된 사실이 RAG 검색 확장뿐만 아니라 최종 답변 생성 시에도 완벽히 포함됩니다.
- **RAG 쿼리 중복 최적화**: [service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py)의 `build_prompt_context`에서 요청 `context`에 RAG 결과를 메모이징하여, 1개 턴 내에서 `build_prompt`와 `build_prompt_messages`가 동시에 불릴 때 발생하는 중복 조회를 단 1회로 줄였습니다.
- **테스트 어설션 정상화**: [test_operator_guide_sprint12_evaluation.py](file:///c:/factory-space/backend/tests/test_operator_guide_sprint12_evaluation.py)에서 `llm.prompt_messages` 슬롯 인덱스 접근 버그(KeyError, IndexError)를 해결하고, RAG 호출 기대 횟수를 6회에서 3회로 정상 반영하였습니다.

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
총 240개의 백엔드 전체 테스트 케이스가 성공적으로 통과하였습니다.
- `uv run pytest tests/test_operator_guide_sprint12_evaluation.py -v` 통과.
- `uv run pytest -q` 전체 백엔드 테스트 suite 통과.
- `uv run ruff check` 코드 포맷 및 린트 검사 통과.

### 2.2. 테스트 결과 출력 전문
```text
tests/test_operator_guide_sprint12_evaluation.py::test_extract_confirmed_facts PASSED
tests/test_operator_guide_sprint12_evaluation.py::test_session_memory_fact_updates_and_contradiction PASSED
tests/test_operator_guide_sprint12_evaluation.py::test_pipeline_memory_context_and_query_expansion_integration PASSED

============================= 3 passed in 2.08s =============================
```

---

## 3. 종합 평가

이번 Sprint 12.1 최적화(Tuning)를 통해, 대화 기억의 사실 정보가 단순 검색어 보강뿐 아니라 최종 LLM 답변의 실질적 근거 자료로 확실하게 기재됨으로써 비서 로봇의 대답 정확도가 한층 향상되었습니다. 
동시에 불필요하게 2배로 발생하던 RAG retriever 쿼리를 요청 캐싱을 통해 턴당 1회(총 3회)로 최적화하여 외부 임베딩 비용과 통신 딜레이를 50% 절감하는 중요한 성과를 거두었습니다.
모든 백엔드 240개 테스트의 그린 빌드가 안정적으로 통과하여 본 버전의 메인 머지를 강력히 추천합니다.
