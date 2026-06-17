"""Manual Q&A RAG 문서를 PostgreSQL + pgvector에 저장하는 모듈.

초보자용 설명:
    `rag_ingestion.py`에서 만든 record를 실제 DB 테이블에 넣거나 갱신한다.
    content hash를 이용해 바뀐 문서만 update하고, 사라진 문서는 비활성화한다.
"""

from __future__ import annotations

from sqlalchemy import Engine, Select, create_engine, func, select, update
from sqlalchemy.dialects.postgresql import insert

from agents.operator_guide.rag_ingestion import (
    FailedIngestionRow,
    ManualRagIngestionRecord,
)
from agents.operator_guide.rag_retriever import ManualRagSearchResult
from agents.operator_guide.rag_schema import (
    manual_rag_documents,
    manual_rag_ingestion_failed_rows,
    manual_rag_ingestion_runs,
)


class SqlAlchemyManualRagStore:
    """SQLAlchemy를 사용해 RAG 문서를 DB에 저장하는 저장소 클래스."""

    def __init__(self, engine: Engine) -> None:
        self._engine = engine

    def list_active_content_hashes(self) -> dict[str, str]:
        """현재 DB에 살아 있는 문서의 content hash를 doc_id 기준으로 가져온다."""

        statement: Select[tuple[str, str]] = select(
            manual_rag_documents.c.doc_id,
            manual_rag_documents.c.content_hash,
        ).where(manual_rag_documents.c.is_active.is_(True))
        with self._engine.begin() as connection:
            return dict(connection.execute(statement).all())

    def upsert_records(self, records: list[ManualRagIngestionRecord]) -> None:
        """새 문서는 insert하고, 이미 있는 문서는 update한다."""

        if not records:
            return
        values = [_record_to_values(record) for record in records]
        statement = insert(manual_rag_documents).values(values)
        update_values = {
            "source_file": statement.excluded.source_file,
            "source_row_id": statement.excluded.source_row_id,
            "title": statement.excluded.title,
            "content": statement.excluded.content,
            "content_hash": statement.excluded.content_hash,
            "metadata_json": statement.excluded.metadata_json,
            "embedding": statement.excluded.embedding,
            "is_active": True,
            "updated_at": func.now(),
        }
        statement = statement.on_conflict_do_update(
            index_elements=[manual_rag_documents.c.doc_id],
            set_=update_values,
        )
        with self._engine.begin() as connection:
            connection.execute(statement)

    def deactivate_missing(self, doc_ids: list[str]) -> None:
        """CSV에서 사라진 문서를 삭제하지 않고 inactive 상태로 바꾼다."""

        if not doc_ids:
            return
        statement = (
            update(manual_rag_documents)
            .where(manual_rag_documents.c.doc_id.in_(doc_ids))
            .values(is_active=False, updated_at=func.now())
        )
        with self._engine.begin() as connection:
            connection.execute(statement)

    def start_ingestion_run(self, run_id: str, source_version: str | None) -> None:
        """Ingestion 실행 시작 정보를 DB에 생성합니다.

        초보자용 설명:
            Ingestion이 구동되기 시작할 때 상태를 'started'로 하여 신규 이력 레코드를 남깁니다.
        """
        statement = insert(manual_rag_ingestion_runs).values(
            run_id=run_id,
            status="started",
            source_version=source_version,
            started_at=func.now(),
        )
        with self._engine.begin() as connection:
            connection.execute(statement)

    def complete_ingestion_run(
        self,
        run_id: str,
        status: str,
        inserted: int,
        updated: int,
        skipped: int,
        deactivated: int,
        failed: int,
        error_message: str | None = None,
    ) -> None:
        """Ingestion 실행 완료 정보와 요약 통계를 업데이트합니다.

        초보자용 설명:
            Ingestion 완료 시(성공 또는 전체 실패), 통계 데이터 및 완료 시각을 기록합니다.
        """
        statement = (
            update(manual_rag_ingestion_runs)
            .where(manual_rag_ingestion_runs.c.run_id == run_id)
            .values(
                status=status,
                inserted=inserted,
                updated=updated,
                skipped=skipped,
                deactivated=deactivated,
                failed=failed,
                error_message=error_message,
                completed_at=func.now(),
            )
        )
        with self._engine.begin() as connection:
            connection.execute(statement)

    def record_failed_rows(self, run_id: str, failed_rows: list[FailedIngestionRow]) -> None:
        """실패한 개별 Row들의 세부 원인을 DB에 적재합니다.

        초보자용 설명:
            부분 실패한 Row들의 정보(파일 정보, 에러 메시지 등)를 일괄로 데이터베이스에 저장합니다.
        """
        if not failed_rows:
            return
        values = [
            {
                "run_id": run_id,
                "source_file": row.source_file,
                "source_row_id": row.source_row_id,
                "title": row.title,
                "error_message": row.error_message,
                "failed_at": func.now(),
            }
            for row in failed_rows
        ]
        statement = insert(manual_rag_ingestion_failed_rows).values(values)
        with self._engine.begin() as connection:
            connection.execute(statement)

    def search_similar(
        self,
        query_embedding: list[float],
        *,
        top_k: int,
    ) -> list[ManualRagSearchResult]:
        """질문 embedding과 가장 가까운 active RAG 문서를 찾는다.

        pgvector의 cosine distance는 값이 작을수록 더 비슷하다.
        응답에서는 사람이 이해하기 쉽게 `score = 1 - distance` 형태로 반환한다.
        """

        if top_k <= 0:
            return []

        distance = manual_rag_documents.c.embedding.cosine_distance(
            query_embedding,
        ).label("distance")
        statement = (
            select(
                manual_rag_documents.c.doc_id,
                manual_rag_documents.c.title,
                manual_rag_documents.c.content,
                manual_rag_documents.c.source_file,
                manual_rag_documents.c.source_row_id,
                manual_rag_documents.c.metadata_json,
                distance,
            )
            .where(manual_rag_documents.c.is_active.is_(True))
            .order_by(distance)
            .limit(top_k)
        )
        with self._engine.begin() as connection:
            rows = connection.execute(statement).all()

        return [
            ManualRagSearchResult(
                doc_id=row.doc_id,
                title=row.title,
                content=row.content,
                source_file=row.source_file,
                source_row_id=row.source_row_id,
                metadata=dict(row.metadata_json),
                score=1.0 - float(row.distance),
            )
            for row in rows
        ]


def create_manual_rag_store(database_url: str) -> SqlAlchemyManualRagStore:
    """DB URL로 PostgreSQL 기반 RAG 저장소를 만든다."""

    return SqlAlchemyManualRagStore(create_engine(database_url))


def _record_to_values(record: ManualRagIngestionRecord) -> dict[str, object]:
    """ingestion record를 SQLAlchemy insert 값으로 바꾼다."""

    return {
        "doc_id": record.doc_id,
        "source_file": record.source_file,
        "source_row_id": record.source_row_id,
        "title": record.title,
        "content": record.embedding_text,
        "content_hash": record.content_hash,
        "metadata_json": record.metadata,
        "embedding": record.embedding,
        "is_active": True,
    }
