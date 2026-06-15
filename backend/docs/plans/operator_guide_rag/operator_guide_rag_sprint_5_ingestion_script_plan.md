# operator_guide RAG Sprint 5 Ingestion Script 계획

## 목표

CSV 기반 RAG 문서를 embedding한 뒤 PostgreSQL/pgvector 저장소에 upsert하는 실행 경로를 만든다.

사용자는 CSV를 수정한 뒤 먼저 dry-run으로 변경 예정 내역을 확인한다.
결과가 괜찮으면 두 번째 명령어로 실제 PostgreSQL/pgvector에 반영한다.

```powershell
# 1. 미리보기: DB는 바뀌지 않음
uv run --env-file .env python scripts/ingest_manual_rag.py --dry-run

# 2. 실제 반영: DB에 저장됨
uv run --env-file .env python scripts/ingest_manual_rag.py
```

## 구현 범위

- ingestion upsert 결과 요약 모델
- 기존 저장소 상태와 새 ingestion record 비교
- `insert`, `update`, `skip`, `deactivate` 계산
- `--dry-run`에서는 DB 변경 없이 요약만 출력
- 실제 실행에서는 변경된 row만 upsert하고 사라진 row는 inactive 처리
- `backend/scripts/ingest_manual_rag.py` CLI 추가

## TDD 검증 순서

1. dry-run이 저장소를 변경하지 않고 summary만 반환하는 테스트를 먼저 작성한다.
2. content_hash가 같은 문서는 skip되는 테스트를 작성한다.
3. content_hash가 달라진 문서는 update되는 테스트를 작성한다.
4. CSV에서 사라진 기존 문서는 deactivate되는 테스트를 작성한다.
5. CLI가 dry-run 후 실제 실행 명령어를 안내하는 테스트를 작성한다.

## 완료 기준

- `uv run pytest tests/test_operator_guide_rag_upsert.py tests/test_ingest_manual_rag_script.py -q` 통과
- 기존 RAG ingestion 테스트 통과
- dry-run 출력에 `inserted`, `updated`, `skipped`, `deactivated`, `failed`가 포함된다.
- dry-run 출력에 실제 반영 명령어가 포함된다.

## 작업 로그

- 2026-06-10: Sprint 5 구현 전 세부 계획을 작성했다.
- 2026-06-10: `ManualRagUpsertService`를 추가해 insert/update/skip/deactivate 판단을 분리했다.
- 2026-06-10: `SqlAlchemyManualRagStore`를 추가해 PostgreSQL `manual_rag_documents` upsert와 inactive 처리를 연결했다.
- 2026-06-10: `backend/scripts/ingest_manual_rag.py`를 추가해 `--dry-run`, `--force`, `--data-dir` 옵션을 제공했다.
- 2026-06-10: Sprint 5 관련 테스트 11개와 ruff 검사를 통과했다.
- 2026-06-10: 실제 실행 명령어가 env 파일을 로드하도록 `--env-file .env` 기준으로 문서를 보완했다.

## 트러블슈팅 로그

- 2026-06-10: 실제 PostgreSQL 연결 없이도 TDD가 가능하도록 upsert 판단 로직을 저장소 인터페이스 뒤에 둔다.
- 2026-06-10: 초기 구현이 skip될 문서까지 모두 embedding할 수 있어, `build_batch`에서 content_hash를 먼저 비교하고 변경된 문서만 embedding하도록 수정했다.
- 2026-06-10: dry-run은 DB 변경뿐 아니라 embedding API 호출도 하지 않도록 분리했다.
