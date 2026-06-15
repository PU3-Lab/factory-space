# operator_guide RAG PostgreSQL + pgvector schema 계획

## 목표

Sprint 4 범위로 operator_guide RAG 문서를 저장할 PostgreSQL + pgvector schema와 Alembic migration 기반을 추가한다.

이번 단계는 실제 ingestion script나 retriever를 붙이기 전, DB에 어떤 형태로 RAG 문서를 저장할지 계약을 고정하는 작업이다.

## 범위

1. Alembic 설정을 추가한다.
2. `manual_rag_documents` SQLAlchemy table metadata를 추가한다.
3. pgvector extension과 vector column을 포함한 최초 migration을 추가한다.
4. schema/migration은 실제 DB 없이도 테스트로 검증한다.

## 제외 범위

- 실제 PostgreSQL 서버 실행
- ingestion script
- OpenAI embedding 호출
- pgvector similarity search
- operator_guide runtime 연결

## 예상 의존성

- `alembic`
- `sqlalchemy`
- `psycopg`
- `pgvector`

## 예상 env

```env
DATABASE_URL=postgresql+psycopg://user:password@localhost:5432/factory_space
```

## 예상 테이블

```text
manual_rag_documents
- id
- doc_id
- source_file
- source_row_id
- title
- content
- content_hash
- metadata jsonb
- embedding vector(1536)
- is_active
- created_at
- updated_at
```

## 인덱스/제약

```text
unique(doc_id)
index(content_hash)
index(is_active)
jsonb index(metadata)
pgvector ivfflat index on embedding
```

## 구현 단계

1. 실패하는 테스트 작성
   - schema metadata가 필요한 컬럼과 인덱스를 가진다.
   - Alembic 설정 파일이 migration 경로를 가리킨다.
   - 최초 migration이 pgvector extension과 `manual_rag_documents` 테이블을 만든다.

2. 최소 구현
   - `rag_schema.py`
   - `alembic.ini`
   - `migrations/env.py`
   - `migrations/versions/0001_create_manual_rag_documents.py`
   - pyproject 의존성 추가

3. 검증
   - Sprint 4 단위 테스트
   - RAG 관련 테스트
   - ruff
   - lockfile 갱신

## 진행 로그

- Sprint 4 시작.
- 기존 backend에는 Alembic/migration 구조가 없는 것을 확인했다.
- `test_operator_guide_rag_pgvector_schema.py`를 먼저 작성했다.
- `pyproject.toml`에 Alembic/PostgreSQL/pgvector 관련 의존성을 추가했다.
- `rag_schema.py`에 `manual_rag_documents` SQLAlchemy metadata를 추가했다.
- `alembic.ini`, `migrations/env.py`, `migrations/versions/0001_create_manual_rag_documents.py`를 추가했다.
- `uv lock`으로 lockfile을 갱신했다.

## 트러블슈팅 로그

### 1. `rag_schema` 모듈 없음

- 문제: Sprint 4 테스트 최초 실행 시 `ModuleNotFoundError: No module named 'agents.operator_guide.rag_schema'` 발생.
- 원인: RED 단계로 테스트를 먼저 작성했고 schema 구현 파일이 아직 없었다.
- 해결: `rag_schema.py`를 추가하고 `manual_rag_documents` metadata를 구현했다.
- 검증: Sprint 4 테스트에서 schema import 성공.

### 2. SQLAlchemy Index의 PostgreSQL 옵션 접근 실패

- 문제: 테스트에서 `index.postgresql_using` 속성 접근 시 `AttributeError` 발생.
- 원인: SQLAlchemy `Index`의 dialect option은 직접 속성이 아니라 `dialect_options["postgresql"]`에 저장된다.
- 해결: 테스트를 SQLAlchemy API에 맞춰 `dialect_options["postgresql"]["using"]` 확인으로 보정했다.
- 검증: Sprint 4 테스트 통과.

### 3. migration import 정렬 ruff 실패

- 문제: `migrations/versions/0001_create_manual_rag_documents.py`에서 ruff `I001` 발생.
- 원인: import block 정렬이 ruff 규칙과 맞지 않았다.
- 해결: import 순서를 정리했다.
- 검증: ruff 통과.

## 검증 로그

- `uv run --extra dev pytest tests/test_operator_guide_rag_pgvector_schema.py -q`
  - 최초 실행: `rag_schema` 모듈 없음으로 실패 확인
  - 구현 후: `4 passed`

- `uv run --extra dev pytest tests/test_operator_guide_rag_documents.py tests/test_operator_guide_rag_ingestion.py tests/test_operator_guide_rag_embedding.py tests/test_operator_guide_rag_pgvector_schema.py -q`
  - `16 passed`

- `uv run --extra dev ruff check src/agents/operator_guide/rag_schema.py src/agents/operator_guide/rag_embedding.py src/agents/operator_guide/csv_repository.py tests/test_operator_guide_rag_pgvector_schema.py tests/test_operator_guide_rag_embedding.py migrations/env.py migrations/versions/0001_create_manual_rag_documents.py`
  - 최초 실행: migration import 정렬 실패
  - 보정 후: `All checks passed!`

- `uv run --extra dev pytest -q --ignore=tests/test_manual_qa_docs_router.py`
  - `172 passed`
