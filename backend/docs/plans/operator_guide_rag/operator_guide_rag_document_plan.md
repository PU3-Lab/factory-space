# operator_guide RAG 문서 변환 계획

## 목표

operator_guide의 CSV 기반 매뉴얼 데이터를 PostgreSQL + pgvector RAG 구조로 옮기기 전에, CSV row를 embedding과 vector 저장에 적합한 문서 단위로 정규화한다.

이번 PR의 목표는 DB 연결이 아니라, RAG 저장소에 넣을 수 있는 안정적인 문서 모델과 CSV 변환 규칙을 먼저 고정하는 것이다.

## 배경

현재 operator_guide는 `data/game` 아래의 5개 CSV를 직접 읽어 질문 분류와 LLM prompt 근거를 만든다.

- `equipment.csv`
- `resources.csv`
- `recipes.csv`
- `troubleshooting_rules.csv`
- `action_policy.csv`

다음 단계에서는 이 CSV row들을 embedding한 뒤 PostgreSQL + pgvector에 저장하고, 플레이어 질문을 embedding 검색해서 관련 근거를 가져오는 RAG 흐름으로 확장한다.

## 이번 PR 범위

1. CSV row 하나를 RAG 문서 하나로 변환한다.
2. RAG 문서는 source 추적이 가능해야 한다.
3. embedding 대상 텍스트는 사람이 읽어도 의미가 분명한 content로 만든다.
4. 기존 CSV 원본은 수정하지 않는다.
5. PostgreSQL, pgvector, embedding 생성, LLM 답변 연결은 다음 PR로 분리한다.

## 문서 모델

`ManualRagDocument`는 다음 필드를 가진다.

- `doc_id`: RAG 문서 고유 id
- `source_file`: 원본 CSV 파일명
- `source_row_id`: 원본 row id
- `title`: 문서 제목
- `content`: embedding에 사용할 정규화 텍스트
- `metadata`: record type 등 필터링에 필요한 메타데이터

## 구현 단계

1. 실패하는 테스트 작성
   - 5개 CSV row가 RAG 문서로 변환되는지 확인한다.
   - `equipment_smelter` 문서가 장비 역할, 입력/출력 자원, 전력 요구량, 관련 문제를 포함하는지 확인한다.
   - `issue_machine_stopped` 문서가 증상, 원인, 확인 순서, 추천 액션, 해결 내용을 포함하는지 확인한다.

2. CSV repository 확장
   - 기존 단건 조회 메서드는 유지한다.
   - RAG 변환 전용으로 전체 row를 반환하는 `list_*` 메서드를 추가한다.

3. RAG 문서 변환기 추가
   - `ManualRagDocument`
   - `ManualRagDocumentBuilder`
   - 장비, 자원, 레시피, 트러블슈팅, 액션 row별 content 생성 규칙

4. 검증
   - 신규 RAG 문서 테스트 통과
   - 기존 Manual Q&A smoke 테스트 통과
   - 관련 파일 ruff 통과
   - untracked manual docs 테스트를 제외한 backend 테스트 통과

## 검증 로그

- `uv run --extra dev pytest tests/test_operator_guide_rag_documents.py -q`
  - 최초 실행: `agents.operator_guide.rag_documents` 모듈 없음으로 실패 확인
  - 구현 후: 통과

- `uv run --extra dev pytest tests/test_operator_guide_rag_documents.py tests/test_manual_qa_agent_smoke.py -q`
  - `16 passed`

- `uv run --extra dev ruff check src/agents/operator_guide/rag_documents.py src/agents/operator_guide/csv_repository.py tests/test_operator_guide_rag_documents.py tests/test_manual_qa_agent_smoke.py`
  - `All checks passed!`

- `uv run --extra dev pytest -q --ignore=tests/test_manual_qa_docs_router.py`
  - `159 passed`

## 다음 PR

다음 PR에서는 이번 문서 모델을 기반으로 PostgreSQL + pgvector 저장소를 붙인다.

예상 작업:

1. PostgreSQL 접속 설정 추가
2. pgvector extension 및 manual RAG document 테이블 설계
3. embedding adapter 추가
4. CSV RAG 문서 ingestion script 추가
5. 질문 embedding 후 similarity search 테스트 추가
