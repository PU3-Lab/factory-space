# operator_guide RAG Retriever Runtime 연결 계획

## 목표

operator_guide가 플레이어 질문을 받았을 때 PostgreSQL + pgvector에 저장된 manual RAG 문서를 검색할 수 있게 한다.

이 단계는 CSV를 실제 DB에 저장하는 ingestion 작업과 분리한다. CSV 내용이 계속 수정 중이므로 실제 embedding 저장은 나중에 수행하고, 지금은 runtime retriever 구조와 테스트 가능한 인터페이스를 먼저 설계한다.

최종 목표는 다음 흐름이다.

```text
Player Question
-> query embedding
-> PostgreSQL + pgvector top-k search
-> active manual documents filtering
-> source metadata formatting
-> confidence/retrieval metadata
-> operator_guide prompt context
```

## 현재 상태

이미 준비된 것:

- `rag_documents.py`: CSV row를 `ManualRagDocument`로 변환
- `rag_embedding.py`: OpenAI/fake/none embedding provider 구조
- `rag_ingestion.py`: 문서 content_hash 계산 및 embedding record 생성
- `rag_store.py`: PostgreSQL + pgvector upsert 저장소
- `rag_upsert.py`: insert/update/skip/deactivate 요약
- `scripts/ingest_manual_rag.py`: dry-run/apply ingestion CLI

아직 필요한 것:

- runtime query embedding
- pgvector similarity search
- top-k 검색 결과 model
- source/context formatter
- confidence 계산과 연결
- operator_guide prompt에 검색 결과 주입

## 이번 Sprint 범위

### 포함 범위

- `ManualRagRetriever` 인터페이스 정의
- query text를 embedding으로 변환
- vector store search protocol 정의
- top-k 결과 반환
- active document만 검색
- source metadata 반환
- context 최대 길이 제한
- 테스트용 fake embedding provider / fake store 구현

### 제외 범위

- 실제 CSV 최종 ingestion 실행
- production DB 튜닝
- reranker
- conversation memory
- prompt injection guardrail 구현
- Unreal UI 표시 구현

prompt injection guardrail은 `operator_guide_prompt_injection_guardrail_plan.md`에 따라 Sprint 8.5에서 구현한다.

## 제안 파일 구조

```text
backend/src/agents/operator_guide/rag_retriever.py
backend/tests/test_operator_guide_rag_retriever.py
```

필요하면 store 검색 메서드는 기존 `rag_store.py`에 추가한다.

```text
SqlAlchemyManualRagStore.search_similar(...)
```

## 데이터 모델 초안

```python
@dataclass(frozen=True)
class ManualRagSearchResult:
    doc_id: str
    title: str
    content: str
    source_file: str
    source_row_id: str
    metadata: dict[str, str]
    score: float


@dataclass(frozen=True)
class ManualRagRetrievalResult:
    query: str
    results: list[ManualRagSearchResult]
    top_score: float | None
    matched_documents: int
    context_text: str
```

## 검색 인터페이스 초안

```python
class ManualRagSearchStore(Protocol):
    def search_similar(
        self,
        query_embedding: list[float],
        *,
        top_k: int,
    ) -> list[ManualRagSearchResult]:
        ...


class ManualRagRetriever:
    def retrieve(self, query: str) -> ManualRagRetrievalResult:
        ...
```

## env 초안

```env
FACTORY_RAG_TOP_K=5
FACTORY_RAG_MAX_CONTEXT_CHARS=6000
```

## Runtime 흐름

```text
question_type_router_node
-> context_need_classifier_node
-> current_game_state_node
-> rag_retriever_node
   - query embedding 생성
   - pgvector top-k 검색
   - active document 필터링
   - source metadata 정리
   - context_text 생성
-> source_formatter_node
-> llm_answer_generator_node
```

## 테스트 계획

TDD 순서:

1. fake embedding provider가 query를 embedding으로 변환했는지 검증
2. fake store가 top-k 검색 요청을 받았는지 검증
3. inactive document가 결과에 포함되지 않는지 검증
4. 검색 결과가 source metadata를 포함하는지 검증
5. context_text가 `FACTORY_RAG_MAX_CONTEXT_CHARS`를 넘지 않는지 검증
6. 검색 결과가 없을 때 `matched_documents=0`, `top_score=None`을 반환하는지 검증

## 완료 기준

- 질문 문자열을 넣으면 `ManualRagRetrievalResult`가 반환된다.
- 결과에는 `doc_id`, `title`, `content`, `source_file`, `source_row_id`, `metadata`, `score`가 포함된다.
- 검색 결과가 없을 때도 안전하게 빈 결과를 반환한다.
- context 길이가 설정값을 넘지 않는다.
- 실제 DB 없이 fake provider/store로 unit test가 가능하다.
- 추후 operator_guide prompt builder가 `context_text`를 그대로 사용할 수 있다.

## 작업 로그

- 2026-06-11: `.env.prod` 기준 RAG dry-run 실행 조건을 확인했다.
- 2026-06-11: `uv run --env-file .env.prod python scripts/ingest_manual_rag.py --dry-run` 실행 결과 `inserted=142, updated=0, skipped=0, deactivated=0, failed=0`을 확인했다.
- 2026-06-11: CSV가 계속 수정 중이므로 실제 ingestion은 미루고 runtime retriever 구조를 먼저 설계하기로 했다.

## 트러블슈팅 로그

- 2026-06-11: Docker 상태 확인에서 Docker API 권한 문제가 발생했다. dry-run은 DB 접속이 가능해 성공했으므로, Docker 상태 확인은 이번 단계의 blocker로 보지 않았다.
- 2026-06-11: `--dry-run`도 기존 content_hash 비교를 위해 `FACTORY_DATABASE_URL`이 필요하다는 점을 확인했다.
