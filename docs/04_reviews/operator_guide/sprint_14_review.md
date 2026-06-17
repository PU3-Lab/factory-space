# 코드 리뷰: operator_guide RAG Sprint 14 (RAG Ingestion 운영 및 관리 확장)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-runtime-docs` |
| 리뷰 일자 | 2026-06-16 |
| 리뷰 범위 | RAG Ingestion 실행 이력 로깅, 부분 실패(Partial Failure) Retry, source_version 연동 |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- **운영 기록 스키마 정의**: [rag_schema.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_schema.py)에 `manual_rag_ingestion_runs`(실행 단위 통계 및 상태 기록)와 `manual_rag_ingestion_failed_rows`(실패한 개별 Row의 상세 원인 기록) 테이블 설정을 추가했습니다.
- **Alembic 마이그레이션**: 신규 테이블을 데이터베이스에 구축하기 위한 [0003_create_manual_rag_ingestion_tables.py](file:///c:/factory-space/backend/migrations/versions/0003_create_manual_rag_ingestion_tables.py) 마이그레이션 스크립트를 작성하여 반영 완료했습니다.
- **Partial Failure & Partial Retry 메커니즘**:
  - [rag_ingestion.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_ingestion.py)의 `ManualRagIngestionService`에서 일괄 임베딩이 실패하는 예외 발생 시, 각 문서를 순회하며 개별 호출을 수행하는 **Partial Retry** 로직을 도입했습니다.
  - 임베딩에 최종 실패한 Row는 `FailedIngestionRow`로 식별하여 데이터베이스에 실패 사유와 함께 격리 기록하고, 성공한 나머지 정상 Row들은 트랜잭션 내에서 문제없이 Upsert 처리합니다.
- **CSV 변경 및 버전 연동**:
  - `calculate_source_version` 유틸리티를 추가하여 현재 CSV 폴더의 Manifest 해시와 Git HEAD 커밋 해시를 조합한 고유 `source_version`을 기록하도록 연동했습니다.
  - [ingest_manual_rag.py](file:///c:/factory-space/backend/scripts/ingest_manual_rag.py) 실행 시 UUID 기반 `run_id`와 버전을 생성해 upsert에 제공합니다.
- **테스트 케이스 추가 및 검증**:
  - [test_operator_guide_rag_sprint14.py](file:///c:/factory-space/backend/tests/test_operator_guide_rag_sprint14.py)를 신설해 버저닝, 부분 실패 격리, DB 기록 적재, 실패 건의 다음번 재시도 편입 등을 포괄적으로 검증하고 린트(ruff) 및 249개 전체 백엔드 테스트의 그린 빌드를 검출했습니다.

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
총 249개의 백엔드 전체 테스트 케이스가 성공적으로 통과하였습니다.
- `uv run pytest tests/test_operator_guide_rag_sprint14.py -v` 통과
- `uv run pytest -q` 전체 백엔드 테스트 suite 통과
- `uv run ruff check` 코드 포맷 및 린트 검사 통과

### 2.2. 테스트 결과 출력 전문
```text
tests/test_operator_guide_rag_sprint14.py::test_source_version_generation PASSED [ 25%]
tests/test_operator_guide_rag_sprint14.py::test_partial_failure_retry_mechanism PASSED [ 50%]
tests/test_operator_guide_rag_sprint14.py::test_ingestion_run_logging_in_db PASSED [ 75%]
tests/test_operator_guide_rag_sprint14.py::test_auto_retry_on_next_run PASSED [100%]

============================== 4 passed in 0.50s ==============================
```

---

## 3. 종합 평가

이번 Sprint 14 작업을 통해, 단순 데이터 적재를 넘어 운영 가시성과 내결함성을 가진 실무용 RAG Ingestion 파이프라인으로 진화하였습니다.
특히 일시적인 네트워크 불통이나 특정 모델의 누락으로 전체 인덱싱이 중단되는 문제를 Partial Retry와 격리 저장을 통해 원천 해결하였으며, `source_version`과 실행 이력(`run_id`) 테이블을 통해 데이터의 정합성 유무와 갱신 연혁을 누구나 손쉽게 추적할 수 있도록 완성도를 확보하였습니다.
모든 단위 테스트와 통합 테스트가 안정적으로 통과하여 본 변경 사항의 머지를 승인합니다.
