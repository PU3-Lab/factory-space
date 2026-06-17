# operator_guide RAG Sprint 14 기획서 (RAG Ingestion 운영 및 관리 확장)

## 1. 개요
지금까지의 RAG Ingestion은 로컬 환경에서 단발성 스크립트 실행을 통해 "동작 가능함"을 검증하는 수준이었습니다. 하지만 실제 서비스 운영 단계에서는 CSV 파일의 변경 이력, Ingestion 실행 이력(성공/실패 이력), 특정 Row의 임베딩 실패(partial failure) 시 상세 원인 기록 및 리트라이 등 운영 관점에서의 신뢰성과 가시성이 필수적입니다.

따라서 Sprint 14에서는 Ingestion 과정을 추적하는 DB 테이블을 도입하고, 부분 실패(partial failure) 발생 시에도 프로세스가 전체 중단되지 않고 성공한 데이터는 정상 반영되며 실패한 데이터만 로그 및 DB에 기록되어 다음 실행 시 자동으로 재시도(retry)되는 고도화된 운영 체계를 구축합니다.

## 2. 목표
- `manual_rag_ingestion_runs` 및 `manual_rag_ingestion_failed_rows` 테이블을 신설하여 Ingestion 이력과 실패 Row를 기록합니다.
- `source_version` 개념을 도입하여 CSV의 내용 상태(Manifest Hash) 및 Git 버전 정보(Git Commit Hash)를 추적합니다.
- 부분 실패(partial failure) 처리 지원:
  - 일괄 임베딩 실패 시 개별 문서 단위로 격리 호출 및 리트라이(partial retry)를 수행합니다.
  - 임베딩에 실패한 Row는 실패 이력 테이블에 에러 사유와 함께 기록하고, 성공한 Row는 DB에 정상 반영(Upsert)합니다.
- 실패 기록이 남은 Row는 다음 Ingestion 수행 시 누락 여부를 판단해 자동으로 인덱싱을 재시도합니다.
- Alembic 마이그레이션을 통해 테이블 스키마를 동적으로 구성합니다.

---

## 3. 상세 기획 및 구현 명세

### 3.1. DB 스키마 설계 ([rag_schema.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_schema.py))
두 개의 신규 테이블 스키마 정의를 추가합니다.

#### A. `manual_rag_ingestion_runs` (Ingestion 실행 이력)
- `id`: Primary Key (Integer)
- `run_id`: UUID/String(255) (고유 실행 ID)
- `status`: String(50) ('started', 'success', 'failed')
- `inserted`: Integer (신규 반영 수)
- `updated`: Integer (업데이트 수)
- `skipped`: Integer (스킵 수)
- `deactivated`: Integer (비활성화 수)
- `failed`: Integer (실패 수)
- `source_version`: String(255) (현재 CSV의 Manifest Hash 및 Git 커밋 정보)
- `error_message`: Text (전체 실패 시 에러 사유)
- `started_at`: DateTime (실행 시작 시각)
- `completed_at`: DateTime (실행 완료 시각)

#### B. `manual_rag_ingestion_failed_rows` (실패한 개별 Row 이력)
- `id`: Primary Key (Integer)
- `run_id`: String(255) (실행 ID)
- `source_file`: String(255) (실패한 CSV 파일명)
- `source_row_id`: String(255) (실패한 CSV 내 Row ID)
- `title`: String(255) (실패한 Row의 타이틀)
- `error_message`: Text (실패 원인/예외 메시지)
- `failed_at`: DateTime (실패 발생 시각)

### 3.2. source_version 분석 유틸리티 추가
- CSV 데이터 디렉토리 전체의 파일 해시 합계(Manifest Hash) 및 현재 Git 커밋 해시 정보를 읽어와 고유한 `source_version` 문자열을 반환하는 함수를 구현합니다.
- Git 명령이 작동하지 않는 환경에서는 CSV Manifest Hash를 Fallback으로 사용하여 환경 독립성을 유지합니다.

### 3.3. Partial Failure 및 Partial Retry 구현 ([rag_ingestion.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_ingestion.py))
- `ManualRagIngestionService._build_records_for_documents`를 다음과 같이 보강합니다:
  - 1차로 전체 문서를 일괄 `embed_texts` 호출하여 임베딩을 시도합니다.
  - 일괄 호출이 예외를 던지거나 `[]`를 반환하는 경우, **Partial Retry** 모드로 전환합니다.
  - 각 문서를 하나씩 순회하며 개별 `embed_texts([content])`를 시도합니다.
  - 개별 임베딩 성공 시 정상 `ManualRagIngestionRecord`로 빌드하고, 실패 시 `failed_documents` 목록에 에러 이유와 함께 적재합니다.
  - 빌드 결과와 함께 실패한 문서 정보들을 반환하도록 메서드 시그니처와 리턴 구조를 개선합니다.

### 3.4. Ingestion Run 기록 및 상태 업데이트 ([rag_store.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_store.py) & [rag_upsert.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_upsert.py))
- `SqlAlchemyManualRagStore`에 Ingestion run 기록 저장 및 실패 Row 적재 기능을 추가합니다.
  - `start_ingestion_run(run_id, source_version) -> None`
  - `complete_ingestion_run(run_id, status, summary, error_message=None) -> None`
  - `record_failed_rows(run_id, failed_rows) -> None`
- `ManualRagUpsertService.upsert_batch` 실행 시점에:
  - DB 트랜잭션 범위 내에서 `manual_rag_ingestion_runs` 및 `manual_rag_ingestion_failed_rows`를 함께 insert 합니다.
  - Ingestion 서비스에서 전달된 실패 정보를 토대로 `failed` 카운트를 요약해 최종 run에 저장합니다.

---

## 4. 검증 계획

### 4.1. 유닛 테스트 작성 ([test_operator_guide_rag_sprint14.py](file:///c:/factory-space/backend/tests/test_operator_guide_rag_sprint14.py))
- `test_source_version_generation`: Manifest Hash와 Git 커밋 연동 유틸리티 검증.
- `test_partial_failure_retry_mechanism`: 일부 문서의 임베딩이 실패하는 상황에서 성공한 문서만 Record로 반환되고 실패한 문서는 누락 정보와 함께 에러 내용이 분리되는지 모킹 검증.
- `test_ingestion_run_logging_in_db`: DB 내에 Ingestion run 이력 및 실패 Row 정보가 스키마에 정의된 구조대로 정확히 적재되는지 검증.
- `test_auto_retry_on_next_run`: 이전 실행에서 실패하여 DB에 적재되지 못한 Row가 다음번 Ingestion 시 Skip되지 않고 다시 임베딩 대상(Retry)에 편입되는지 검증.

### 4.2. 실행 및 린터 검증
```powershell
# 1. 마이그레이션 실행
uv run --env-file .env alembic revision --autogenerate -m "create_manual_rag_ingestion_tables"
uv run --env-file .env alembic upgrade head

# 2. 테스트 및 린터 실행
uv run pytest tests/test_operator_guide_rag_sprint14.py -v
uv run pytest -q
uv run ruff check
```
