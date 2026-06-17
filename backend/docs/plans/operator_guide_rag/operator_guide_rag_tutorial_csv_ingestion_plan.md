# operator_guide RAG tutorial.csv ingestion 계획

## 목적

`data/game/tutorial.csv`가 새로 추가되었으므로, 기존 RAG ingestion 흐름에 튜토리얼 CSV도 포함한다.

현재 ingestion 명령은 `data/game` 폴더를 기준으로 동작하지만, 문서 변환 단계가 장비, 자원, 레시피, 트러블슈팅, 액션 정책 CSV만 RAG 문서로 만들고 있다. 따라서 `tutorial.csv`를 PostgreSQL + pgvector에 저장하려면 먼저 튜토리얼 row를 `ManualRagDocument`로 변환해야 한다.

## 범위

- `CsvManualQARepository`에 tutorial CSV reader 추가
- `ManualRagDocumentBuilder`에 tutorial 문서 변환 추가
- tutorial 문서 변환 테스트 추가
- dry-run으로 변경 대상 확인
- 실제 ingestion 실행

## 완료 기준

```text
- tutorial.csv row가 RAG 문서 목록에 포함된다.
- tutorial 문서의 source_file은 tutorial.csv로 기록된다.
- dry-run에서 tutorial 문서가 추가/변경 대상으로 보인다.
- 실제 ingestion이 PostgreSQL + pgvector에 저장까지 완료된다.
```

## 검증 계획

```powershell
cd C:\factory-space\backend
uv run pytest tests/test_operator_guide_rag_documents.py -q
uv run ruff check .
uv run --env-file .env.prod python scripts/ingest_manual_rag.py --dry-run
uv run --env-file .env.prod python scripts/ingest_manual_rag.py
```

## 작업 로그

- 2026-06-16: `data/game/tutorial.csv`가 존재하지만 기존 RAG 문서 변환 대상에는 포함되지 않는 것을 확인했다.
- 2026-06-16: `tutorial.csv`를 읽는 repository record와 RAG 문서 변환 로직을 추가했다.
- 2026-06-16: Docker Desktop을 실행하고 `docker-compose.rag.yml`의 PostgreSQL + pgvector 컨테이너를 시작했다.
- 2026-06-16: `uv run pytest tests/test_operator_guide_rag_documents.py -q` 결과 4개 테스트가 통과했다.
- 2026-06-16: `uv run ruff check .` 결과 통과했다.
- 2026-06-16: `uv run --env-file .env.prod alembic upgrade head` 결과 migration 적용이 완료됐다.
- 2026-06-16: `uv run --env-file .env.prod python scripts/ingest_manual_rag.py --dry-run` 결과 `inserted=58, updated=112, skipped=27, deactivated=3, failed=0`을 확인했다.
- 2026-06-16: `uv run --env-file .env.prod python scripts/ingest_manual_rag.py` 결과 실제 ingestion이 완료됐다.
- 2026-06-16: PostgreSQL에서 `tutorial.csv` 문서 56개가 `manual_rag_documents`에 저장된 것을 확인했다.

## 트러블슈팅 로그

- 2026-06-16: ingestion script는 `data/game`을 기본 입력 폴더로 사용하지만, 실제 embedding 대상은 `ManualRagDocumentBuilder.build_all()`이 반환하는 문서에 한정된다. 따라서 새 CSV 파일은 repository와 document builder에 명시적으로 추가해야 한다.
- 2026-06-16: Alembic migration 중 `0003_create_manual_rag_ingestion_tables` revision id가 Alembic 기본 `version_num VARCHAR(32)`보다 길어 `StringDataRightTruncation`이 발생했다. revision id를 `0003_manual_rag_ingest`로 줄여 version table에 저장 가능하도록 수정했다.
