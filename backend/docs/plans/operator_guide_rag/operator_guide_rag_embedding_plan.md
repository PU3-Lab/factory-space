# operator_guide RAG embedding provider 계획

## 목표

Sprint 3 범위로 embedding 설정과 OpenAI embedding provider를 추가한다.

이번 단계는 `ManualRagIngestionRecord`에 들어갈 embedding vector를 실제 provider에서 받을 수 있게 하는 작업이다. PostgreSQL 저장소와 pgvector 검색은 다음 Sprint로 분리한다.

## 범위

1. embedding env 설정을 읽는다.
2. `OpenAIEmbeddingProvider`를 추가한다.
3. 기본 모델은 `text-embedding-3-small`로 둔다.
4. 테스트에서는 실제 OpenAI API를 호출하지 않고 fake client로 검증한다.
5. local embedding provider는 후속 Sprint로 분리한다.

## 제외 범위

- PostgreSQL 접속
- pgvector schema
- ingestion script
- local embedding provider
- operator_guide runtime 연결

## 예상 env

```env
FACTORY_EMBEDDING_PROVIDER=openai
FACTORY_EMBEDDING_MODEL=text-embedding-3-small
FACTORY_EMBEDDING_DIMENSIONS=1536
```

`FACTORY_EMBEDDING_API_KEY`가 있으면 우선 사용하고, 없으면 `OPENAI_API_KEY`를 사용한다.

## 구현 단계

1. 실패하는 테스트 작성
   - env에서 embedding 설정을 읽는다.
   - OpenAI provider가 fake client에 올바른 payload를 보낸다.
   - provider가 none이면 빈 embedding 결과를 반환한다.

2. 최소 구현
   - `EmbeddingSettings`
   - `OpenAIEmbeddingProvider`
   - `NoopEmbeddingProvider`
   - `create_embedding_provider`

3. 검증
   - Sprint 3 단위 테스트
   - 기존 RAG ingestion/document 테스트
   - ruff

## 진행 로그

- Sprint 3 시작.
- 기존 LLM OpenAI adapter 패턴을 확인했다.
  - SDK client를 주입 가능하게 만들어 테스트에서 실제 API 호출을 막는 구조를 따른다.
- `test_operator_guide_rag_embedding.py`를 먼저 작성했다.
- `rag_embedding.py`를 추가했다.
  - `EmbeddingSettings`
  - `OpenAIEmbeddingProvider`
  - `NoopEmbeddingProvider`
  - `create_embedding_provider`
- 현재 CSV가 영문 헤더에서 한글 헤더 기반 최종 데이터 형태로 바뀐 것을 확인했다.
  - `csv_repository.py`가 영문/한글 헤더를 모두 읽도록 보강했다.
  - `제련기(equipment_smelter)` 같은 label + id 값을 id 중심으로 정규화하도록 보강했다.

## 트러블슈팅 로그

### 1. `rag_embedding` 모듈 없음

- 문제: Sprint 3 테스트 최초 실행 시 `ModuleNotFoundError: No module named 'agents.operator_guide.rag_embedding'` 발생.
- 원인: RED 단계로 테스트를 먼저 작성했고 구현 파일이 아직 없었다.
- 해결: `backend/src/agents/operator_guide/rag_embedding.py` 추가.
- 검증: `tests/test_operator_guide_rag_embedding.py` 통과.

### 2. RAG document 테스트가 `equipment_id`를 찾지 못함

- 문제: RAG 관련 테스트 묶음 실행 시 `KeyError: 'equipment_id'` 발생.
- 원인: 현재 `data/game/*.csv`가 `equipment_id` 같은 영문 헤더가 아니라 `장비ID`, `자원ID`, `레시피ID` 같은 한글 헤더 기반 최종 데이터 형태로 변경되어 있었다.
- 해결:
  - `csv_repository.py`에 영문/한글 헤더 alias 읽기를 추가했다.
  - `이름(id)` 형태의 CSV 값을 embedding/search에 쓰기 좋은 id 값으로 정규화했다.
  - smoke test helper도 영문/한글 id 헤더를 모두 읽도록 보강했다.
- 검증: Manual Q&A smoke + RAG 테스트 통과.

## 검증 로그

- `uv run --extra dev pytest tests/test_operator_guide_rag_embedding.py -q`
  - 최초 실행: `rag_embedding` 모듈 없음으로 실패 확인
  - 구현 후: `6 passed`

- `uv run --extra dev pytest tests/test_operator_guide_rag_documents.py tests/test_operator_guide_rag_ingestion.py tests/test_operator_guide_rag_embedding.py -q`
  - CSV 헤더 변경으로 최초 `3 failed, 9 passed`
  - CSV repository 보강 후 `12 passed`

- `uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py tests/test_operator_guide_rag_documents.py tests/test_operator_guide_rag_ingestion.py tests/test_operator_guide_rag_embedding.py -q`
  - smoke helper 보강 후 `25 passed`

- `uv run --extra dev ruff check src/agents/operator_guide/csv_repository.py src/agents/operator_guide/rag_embedding.py tests/test_manual_qa_agent_smoke.py tests/test_operator_guide_rag_embedding.py`
  - `All checks passed!`
