# operator_guide RAG Sprint 12.1 기획서 (Memory Prompt & RAG Call Tuning)

## 1. 개요
Sprint 12에서 구현된 세션 대화 기억 기능은 검색 정확도(RAG Query Expansion)를 높이는 데 큰 역할을 했지만, 두 가지 보완해야 할 점이 식별되었습니다:
1. 추출된 확정 사실(`confirmed_facts`)들이 RAG 검색어 확장에는 쓰이나, 최종 LLM 답변 생성 시 명시적인 컨텍스트 근거(`[CONFIRMED_FACTS]`)로 전달되지 않아 추론 시 누락될 위험이 있습니다.
2. 1개 요청 턴 내에서 프롬프트 단일 문자열 빌드와 메시지 리스트 빌드가 연이어 실행되면서, RAG 검색이 불필요하게 2번 실행(3턴 시나리오에서 총 6회 RAG 호출)되는 중복 쿼리 병목이 존재합니다.

따라서 Sprint 12.1에서는 프롬프트 보정 및 RAG 캐싱 튜닝 작업을 수행하여 응답 성능 최적화와 품질 보강을 달성합니다.

## 2. 목표
- LLM용 프롬프트 템플릿에 `[CONFIRMED_FACTS]` 섹션 추가 및 누적 사실 명시적 주입.
- 서비스 레이어의 동일 턴 실행 컨텍스트에 RAG 결과를 메모이징하여 중복 호출을 제거 (턴당 1회, 총 3회 호출로 최적화).
- 테스트 코드에서 `llm.prompt_messages` 인덱스 버그(KeyError/IndexError)를 수정하고 3회 RAG 호출 검증 통과.

---

## 3. 상세 기획 및 구현 명세

### 3.1. 프롬프트 본문 확정 사실 주입 ([prompt_builder.py](file:///c:/factory-space/backend/src/agents/operator_guide/prompt_builder.py))
- [ManualQAPromptBuilder.build_user_prompt](file:///c:/factory-space/backend/src/agents/operator_guide/prompt_builder.py#L61) 내부에 `_confirmed_facts_section` 메소드를 연동합니다.
- 확정 사실 리스트가 존재하면 프롬프트 내에 아래 형식으로 섹션을 빌드해 LLM에 제공합니다:
  ```text
  [CONFIRMED_FACTS]
  - 컨베이어가 멈춤
  - 전력 상태는 정상
  ```

### 3.2. 동일 요청 내 RAG 캐싱 및 중복 제거 ([service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py))
- `ManualQAService.build_prompt_context(..., context)` 내에서 `context` 딕셔너리를 활용해 캐싱을 처리합니다.
- `_cached_rag_result` 키가 `context` 딕셔너리 내에 존재하면 pgvector 검색을 건너뛰고 기존 캐싱 결과를 그대로 반환하여 RAG 런타임 중복 실행을 완벽히 격리시킵니다.

---

## 4. 검증 계획

### 4.1. 통합 테스트 수정 및 확인
- [test_operator_guide_sprint12_evaluation.py](file:///c:/factory-space/backend/tests/test_operator_guide_sprint12_evaluation.py) 내의 LLM 응답 인덱스 검증 로직(`llm.prompt_messages[X][1]["content"]`)이 0, 1, 2 순서로 정렬되도록 버그를 수정합니다.
- `assert len(fake_rag.calls) == 3` 단언을 통해 턴당 1회의 캐싱 최적화 여부를 검증합니다.
- 프롬프트 본문에 `[CONFIRMED_FACTS]`가 유입되는지 문자열 매칭 검증을 수행합니다.

### 4.2. 실행
```powershell
uv run pytest tests/test_operator_guide_sprint12_evaluation.py -v
```
