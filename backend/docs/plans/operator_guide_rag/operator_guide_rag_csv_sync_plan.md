# operator_guide RAG CSV 동기화 계획

## 목표

`data/game`의 5개 CSV가 시간이 지나며 수정될 때, 변경된 내용만 RAG 검색 인덱스에 반영하는 동기화 방식을 정의한다.

## 핵심 원칙

CSV 파일이 수정된다고 자동으로 embedding과 PostgreSQL/pgvector 저장이 일어나는 것은 아니다. CSV 변경을 RAG 저장소에 반영하려면 ingestion trigger가 필요하다.

```text
CSV 수정
-> ingestion script 실행
-> RAG 문서 변환
-> content_hash 비교
-> 변경된 문서만 embedding
-> PostgreSQL/pgvector upsert
```

## 변경 감지 방식

문서 단위의 `content_hash`를 사용한다.

```text
doc_id 같고 content_hash 같음 -> skip
doc_id 같고 content_hash 다름 -> update + re-embed
doc_id 없음 -> insert
DB에는 있는데 CSV에 없음 -> is_active=false
```

이 방식이면 CSV 파일 전체가 바뀌어도 실제 embedding은 변경된 row만 수행한다.

## Ingestion Trigger 단계

## CSV 수정 후 실행 명령어

CSV를 수정한 뒤에는 아래 순서만 기억하면 된다.

```powershell
# 1. 실제 DB를 바꾸기 전에 변경 예정 내역 확인
uv run --env-file .env python scripts/ingest_manual_rag.py --dry-run

# 2. 문제가 없으면 PostgreSQL/pgvector에 실제 반영
uv run --env-file .env python scripts/ingest_manual_rag.py
```

로컬 `.env`를 사용하는 실제 실행 명령어는 다음과 같다.

```powershell
uv run --env-file .env python scripts/ingest_manual_rag.py --dry-run
uv run --env-file .env python scripts/ingest_manual_rag.py
```

`--dry-run` 결과는 다음 행동을 안내해야 한다.

```text
Dry-run complete.
inserted=2, updated=1, skipped=142, deactivated=0

실제로 반영하려면:
uv run --env-file .env python scripts/ingest_manual_rag.py
```

### Sprint 5 기본 방식

수동 실행을 기본으로 한다.

```powershell
uv run --env-file .env python scripts/ingest_manual_rag.py --dry-run
uv run --env-file .env python scripts/ingest_manual_rag.py
```

초기에는 embedding 비용과 DB 변경 위험을 줄이기 위해 자동 watcher를 붙이지 않는다.

### 후속 운영 확장

필요하면 다음 방식으로 자동화한다.

```text
개발용 watcher:
- data/game/*.csv 변경 감지
- ingest 실행

admin endpoint:
- POST /admin/manual-rag/ingest
- 운영자가 명시적으로 re-ingest 실행

CI/CD job:
- CSV 변경 PR merge 후 배포 과정에서 ingest 실행

서버 시작 시 검사:
- CSV hash manifest 확인
- 변경이 있으면 ingest 실행
- 초기 운영에서는 권장하지 않고 후속 옵션으로 둔다.
```

## Ingestion Summary

ingestion 결과는 로그와 metadata로 남긴다.

```json
{
  "inserted": 2,
  "updated": 1,
  "skipped": 142,
  "deactivated": 0,
  "failed": 0
}
```

## 완료 기준

- CSV 변경 시 전체 재임베딩이 아니라 변경된 row만 재임베딩한다.
- 수동 실행과 dry-run이 지원된다.
- 후속 자동화 지점이 문서화된다.
- ingestion 결과가 로그로 남는다.

## 작업 로그

- 2026-06-10: CSV 변경 시 RAG 저장소를 어떻게 동기화할지 계획을 작성했다.
- 2026-06-10: CSV 수정 후 실행할 dry-run/apply 명령어를 운영 절차로 추가했다.
- 2026-06-10: Sprint 5 구현에서 dry-run/apply 명령어의 실제 실행 스크립트를 추가했다.

## 트러블슈팅 로그

- 2026-06-10: CSV 변경만으로 자동 embedding이 수행된다고 오해되지 않도록 ingestion trigger가 필요하다고 명시했다.
- 2026-06-10: 전체 CSV를 매번 embedding하지 않도록 content_hash 비교 후 변경 문서만 embedding하는 흐름으로 정리했다.
