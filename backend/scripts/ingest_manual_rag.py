"""Ingest Manual Q&A CSV rows into PostgreSQL + pgvector."""

from __future__ import annotations

import argparse
import os
import uuid
from collections.abc import Sequence
from pathlib import Path
from typing import TextIO

from sqlalchemy.exc import SQLAlchemyError

from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.rag_documents import ManualRagDocumentBuilder
from agents.operator_guide.rag_embedding import create_embedding_provider
from agents.operator_guide.rag_ingestion import (
    ManualRagIngestionService,
    calculate_source_version,
)
from agents.operator_guide.rag_store import create_manual_rag_store
from agents.operator_guide.rag_upsert import (
    ManualRagUpsertService,
    ManualRagUpsertSummary,
)


def main(argv: Sequence[str] | None = None, output: TextIO | None = None) -> int:
    args = _parse_args(argv)
    out = output
    database_url = _database_url()
    if database_url is None:
        raise RuntimeError(
            "FACTORY_RAG_DATABASE_URL or FACTORY_DATABASE_URL is required."
        )

    data_directory = _data_dir(args.data_dir)
    repository = CsvManualQARepository(data_directory)
    documents = ManualRagDocumentBuilder(repository).build_all()
    store = create_manual_rag_store(database_url)
    try:
        existing_hashes = store.list_active_content_hashes()
    except SQLAlchemyError:
        print(format_database_error(), file=out)
        return 2

    run_id = str(uuid.uuid4())
    actual_data_dir = repository._data_dir
    source_version = calculate_source_version(actual_data_dir)

    embedding_provider = create_embedding_provider()
    batch = ManualRagIngestionService(embedding_provider).build_batch(
        documents,
        existing_content_hashes=existing_hashes,
        dry_run=args.dry_run,
        force=args.force,
    )
    summary = ManualRagUpsertService(store).upsert_batch(
        batch,
        run_id=run_id,
        source_version=source_version,
        dry_run=args.dry_run,
        force=args.force,
    )
    print(format_summary(summary, dry_run=args.dry_run), file=out)
    return 0


def format_summary(summary: ManualRagUpsertSummary, *, dry_run: bool) -> str:
    status = "Dry-run complete." if dry_run else "Ingestion complete."
    lines = [
        status,
        (
            f"inserted={summary.inserted}, "
            f"updated={summary.updated}, "
            f"skipped={summary.skipped}, "
            f"deactivated={summary.deactivated}, "
            f"failed={summary.failed}"
        ),
    ]
    if dry_run:
        lines.extend(
            [
                "",
                "실제로 반영하려면:",
                "uv run --env-file .env.prod python scripts/ingest_manual_rag.py",
            ],
        )
    return "\n".join(lines)


def format_database_error() -> str:
    return "\n".join(
        [
            "RAG database connection failed.",
            "",
            "먼저 PostgreSQL + pgvector를 실행하고 migration을 적용해 주세요.",
            "",
            "예시:",
            "docker compose -f docker-compose.rag.yml up -d",
            "uv run --env-file .env.prod alembic upgrade head",
            "uv run --env-file .env.prod python scripts/ingest_manual_rag.py --dry-run",
        ],
    )


def _parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Ingest Manual Q&A CSV rows into PostgreSQL + pgvector.",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=None,
        help="CSV data directory. Defaults to repository data/game.",
    )
    return parser.parse_args(argv)


def _database_url() -> str | None:
    return os.environ.get("FACTORY_RAG_DATABASE_URL") or os.environ.get(
        "FACTORY_DATABASE_URL"
    )


def _data_dir(data_dir: Path | None) -> Path | None:
    if data_dir is None:
        return None
    return data_dir


if __name__ == "__main__":
    raise SystemExit(main())
