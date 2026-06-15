# operator_guide RAG ingestion 계획

## 목표

CSV row를 `ManualRagDocument`로 정규화한 다음 단계로, embedding과 PostgreSQL + pgvector 저장에 넘길 ingestion record 계약을 만든다.

이번 작업은 실제 DB 접속을 붙이기 전 단계다. DB 서버가 없어도 테스트 가능한 형태로 ingestion payload를 고정해, 다음 단계의 PostgreSQL 저장소와 embedding adapter가 같은 계약을 사용하게 한다.

## 이번 범위

1. `ManualRagDocument`를 ingestion record로 변환한다.
2. ingestion record에는 source 추적, content hash, embedding 대상 텍스트가 포함되어야 한다.
3. embedding provider는 아직 실제 API를 호출하지 않고 protocol/fake로 테스트한다.
4. PostgreSQL 접속, pgvector 테이블 생성, 실제 similarity search는 다음 작업으로 분리한다.

## 제외 범위

- PostgreSQL 접속 코드
- pgvector extension 생성
- 실제 OpenAI/Ollama embedding 호출
- operator_guide 최종 답변 경로 교체
- PR 생성/커밋/푸쉬

## 성공 기준

1. CSV RAG 문서가 ingestion record로 변환된다.
2. 같은 문서는 항상 같은 `content_hash`를 가진다.
3. embedding provider가 반환한 vector가 ingestion record에 연결된다.
4. 관련 단위 테스트와 ruff가 통과한다.

## 검증 로그

- `uv run --extra dev pytest tests/test_operator_guide_rag_ingestion.py -q`
  - 최초 실행: `agents.operator_guide.rag_ingestion` 모듈 없음으로 실패 확인
  - 구현 후: `3 passed`

- `uv run --extra dev pytest tests/test_operator_guide_rag_documents.py tests/test_operator_guide_rag_ingestion.py -q`
  - `6 passed`

- `uv run --extra dev ruff check src/agents/operator_guide/rag_documents.py src/agents/operator_guide/rag_ingestion.py tests/test_operator_guide_rag_documents.py tests/test_operator_guide_rag_ingestion.py`
  - `All checks passed!`

## 예상 파일

- `backend/src/agents/operator_guide/rag_ingestion.py`
- `backend/tests/test_operator_guide_rag_ingestion.py`

## 다음 작업

이 계약이 고정되면 다음에는 PostgreSQL + pgvector 저장소를 추가한다.

예상 흐름:

1. `manual_rag_documents` 테이블 설계
2. `content_hash` 기반 upsert
3. `embedding vector(...)` 저장
4. 질문 embedding 후 top-k similarity search

## CSV 변경 동기화 원칙

CSV 파일이 수정된다고 자동으로 embedding과 PostgreSQL/pgvector 저장이 일어나는 것은 아니다. RAG 저장소를 최신 상태로 맞추려면 ingestion script 또는 후속 admin/CI job 같은 trigger가 필요하다.

```text
CSV 수정
-> ingestion trigger 실행
-> RAG document 변환
-> content_hash 비교
-> 변경된 row만 embedding
-> PostgreSQL/pgvector upsert
```

초기 구현에서는 수동 실행과 `--dry-run`을 우선한다. watcher, admin endpoint, CI/CD ingestion job은 운영 확장 단계에서 다룬다.
