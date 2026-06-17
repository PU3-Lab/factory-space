# operator_guide RAG Sprint 12 기획서 (Memory Evaluation & Runtime Tuning)

## 1. 개요
Sprint 9에서 도입된 대화 기억(Session Memory) 기능을 고도화하여, 플레이어의 후속 질문에서 이전의 확인된 사실(Confirmed Facts)을 참고할 수 있도록 합니다. 이를 통해 플레이어가 "그럼 다음은?", "전력은 정상인데?" 같은 맥락 생략형 질문을 하더라도 과거 사실을 기반으로 정확한 매뉴얼을 매칭하도록 RAG 시스템을 튜닝합니다.

## 2. 목표
- 플레이어 질문 텍스트로부터 게임 상태 사실(전력 상태, 컨베이어 상태, 출력 저장고 상태, 라인 정체 등)을 추출하는 규칙 구현.
- 세션별로 추출된 사실들을 누적 보관 및 상충 상태의 갱신 규칙 설계.
- 후속 질문 시 누적 사실 정보를 합산하여 RAG 검색 질의를 늘리는 쿼리 확장(Query Expansion) 연동.
- 응답 메타데이터에 `confirmed_facts` 및 `summary_version` 정보를 snake_case와 camelCase 모두 지원하도록 탑재.
- Windows 환경 테스트 빌드 문제 해결 (SQLite 파일 락 처리).

---

## 3. 상세 기획 및 구현 명세

### 3.1. 확정 사실 추출 및 관리 ([session_memory.py](file:///c:/factory-space/backend/src/agents/operator_guide/session_memory.py))
- `extract_confirmed_facts(question: str) -> list[str]`
  - 플레이어 질문에서 "전력 정상", "전력 부족", "컨베이어 정지/막힘", "저장고 가득참/비어있음", "라인 정체" 등의 문맥을 파싱해 표준화된 팩트 리스트로 반환합니다.
- `OperatorGuideSessionMemory` 상태 업데이트 규칙:
  - 새로운 사실이 기존 리스트에 없으면 덧붙이고, 요약 버전(`summary_version`)을 1 증가시킵니다.
  - 상충되는 사실(예: "전력 상태는 정상" vs "전력 부족")이 인입되는 경우 기존 상태를 제거하고 최신 팩트로 교체한 뒤 요약 버전을 증가시킵니다.

### 3.2. 파이프라인 컨텍스트 연동 ([runtime.py](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py))
- `AgentContext.metadata`에 `confirmed_facts` 리스트를 바인딩하여 서비스 레이어로 전송합니다.
- 최종 응답 메타데이터의 `memory` 필드에 아래 규격을 모두 반환합니다:
  - `confirmed_facts` / `confirmedFacts`
  - `summary_version` / `summaryVersion`

### 3.3. RAG 쿼리 확장 (Query Expansion - [service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py))
- RAG 검색 요청을 전송하기 전, 질문 뒤에 누적된 `confirmed_facts` 문자열을 스페이스 공백으로 합친 확장된 질의(`search_query = f"{question} {' '.join(confirmed_facts)}"`)를 구성하여 pgvector에 검색을 의뢰합니다.

---

## 4. 검증 계획

### 4.1. 단위 및 통합 테스트
- `test_extract_confirmed_facts`: 정해진 사실 파싱 정밀도 테스트.
- `test_session_memory_fact_updates_and_contradiction`: 중복 제거 및 상충 상태 교체 시의 버전 카운트 검증.
- `test_pipeline_memory_context_and_query_expansion_integration`: 3개 대화 턴의 시뮬레이션을 통해 최종 턴 RAG 검색어에 과거 누적 팩트들이 성공적으로 주입되는지 연동 검증.

### 4.2. 실행
```powershell
uv run pytest tests/test_operator_guide_sprint12_evaluation.py -v
uv run pytest tests/test_operator_guide_session_memory.py -v
```
