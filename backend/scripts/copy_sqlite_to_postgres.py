"""Copy the local Factory Space SQLite seed database into PostgreSQL.

This script is intentionally conservative:
- it copies only the application data tables that exist in ``factory_space.db``;
- it skips copying when PostgreSQL already has the same row counts;
- it refuses to overwrite different PostgreSQL data unless ``RESET_POSTGRES_DATA=1``.
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from sqlalchemy import MetaData, Table, create_engine, delete, func, select, text
from sqlalchemy.engine import Connection

BACKEND_ROOT = Path(__file__).resolve().parents[1]
SRC_PATH = BACKEND_ROOT / "src"
if str(SRC_PATH) not in sys.path:
    sys.path.insert(0, str(SRC_PATH))

from db.models import Base  # noqa: E402

DEFAULT_DATABASE_URL = (
    "postgresql+psycopg://factory_space:factory_space"
    "@127.0.0.1:5433/factory_space?connect_timeout=5"
)

COPY_TABLES = [
    "recipes",
    "generated_materials",
    "generated_experiments",
    "generated_material_discoveries",
    "quest_instances",
    "quest_progress",
]

DELETE_TABLES = [
    "quest_progress",
    "quest_instances",
    "generated_material_discoveries",
    "generated_experiments",
    "generated_materials",
    "recipes",
]

SEQUENCE_TABLES = ["recipes"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Copy backend/factory_space.db data into local PostgreSQL.",
    )
    parser.add_argument(
        "--source",
        default=os.environ.get("SOURCE_SQLITE_PATH", "factory_space.db"),
        help="SQLite database path relative to backend/. Defaults to factory_space.db.",
    )
    parser.add_argument(
        "--database-url",
        default=(
            os.environ.get("DATABASE_URL") or DEFAULT_DATABASE_URL
        ),
        help="Target PostgreSQL SQLAlchemy URL.",
    )
    parser.add_argument(
        "--write-env",
        action="append",
        default=[],
        help="Env file path to upsert DATABASE_URL into.",
    )
    parser.add_argument(
        "--reset",
        action="store_true",
        default=os.environ.get("RESET_POSTGRES_DATA") == "1",
        help="Delete copied target tables first when PostgreSQL has different data.",
    )
    return parser.parse_args()


def resolve_backend_path(path_text: str) -> Path:
    path = Path(path_text)
    if path.is_absolute():
        return path
    return BACKEND_ROOT / path


def load_source_tables(source_url: str) -> tuple[object, dict[str, Table]]:
    source_engine = create_engine(source_url)
    source_meta = MetaData()
    source_meta.reflect(source_engine, only=COPY_TABLES)
    missing_tables = [name for name in COPY_TABLES if name not in source_meta.tables]
    if missing_tables:
        joined = ", ".join(missing_tables)
        raise RuntimeError(f"SQLite source is missing required table(s): {joined}")
    return source_engine, source_meta.tables


def target_tables() -> dict[str, Table]:
    tables = {table.name: table for table in Base.metadata.sorted_tables}
    missing_tables = [name for name in COPY_TABLES if name not in tables]
    if missing_tables:
        joined = ", ".join(missing_tables)
        raise RuntimeError(f"Target metadata is missing required table(s): {joined}")
    return tables


def table_count(connection: Connection, table: Table) -> int:
    return connection.execute(select(func.count()).select_from(table)).scalar_one()


def read_counts(
    source_connection: Connection,
    target_connection: Connection,
    source_tables: dict[str, Table],
    target_tables_by_name: dict[str, Table],
) -> dict[str, tuple[int, int]]:
    counts = {}
    for table_name in COPY_TABLES:
        counts[table_name] = (
            table_count(source_connection, source_tables[table_name]),
            table_count(target_connection, target_tables_by_name[table_name]),
        )
    return counts


def has_different_existing_data(counts: dict[str, tuple[int, int]]) -> bool:
    return any(target_count > 0 and source_count != target_count for source_count, target_count in counts.values())


def has_same_existing_data(counts: dict[str, tuple[int, int]]) -> bool:
    return all(source_count == target_count for source_count, target_count in counts.values())


def delete_target_rows(
    target_connection: Connection,
    target_tables_by_name: dict[str, Table],
) -> None:
    for table_name in DELETE_TABLES:
        target_connection.execute(delete(target_tables_by_name[table_name]))
        print(f"{table_name}: deleted target rows")


def row_dicts(source_connection: Connection, table: Table) -> list[dict[str, object]]:
    return [dict(row) for row in source_connection.execute(select(table)).mappings()]


def copy_rows(
    source_connection: Connection,
    target_connection: Connection,
    source_tables: dict[str, Table],
    target_tables_by_name: dict[str, Table],
) -> None:
    for table_name in COPY_TABLES:
        rows = row_dicts(source_connection, source_tables[table_name])
        if rows:
            target_connection.execute(target_tables_by_name[table_name].insert(), rows)
        print(f"{table_name}: copied {len(rows)} rows")


def reset_sequences(connection: Connection) -> None:
    for table_name in SEQUENCE_TABLES:
        max_id = connection.execute(
            text(f"SELECT COALESCE(MAX(id), 0) FROM {table_name}"),
        ).scalar_one()
        if max_id <= 0:
            print(f"{table_name}: sequence unchanged (empty)")
            continue

        sequence_name = connection.execute(
            text("SELECT pg_get_serial_sequence(:table_name, 'id')"),
            {"table_name": table_name},
        ).scalar_one()
        if sequence_name:
            connection.execute(
                text("SELECT setval(:sequence_name, :value, true)"),
                {"sequence_name": sequence_name, "value": max_id},
            )
            print(f"{table_name}: sequence set to {max_id}")


def env_lines_with_database_url(lines: list[str], database_url: str) -> list[str]:
    replacements = {
        "DATABASE_URL": f"DATABASE_URL={database_url}",
    }
    seen = set()
    updated = []

    for line in lines:
        stripped = line.strip()
        matched_key = None
        for key in replacements:
            if stripped.startswith(f"{key}="):
                matched_key = key
                break

        if matched_key is None:
            updated.append(line)
            continue

        updated.append(replacements[matched_key])
        seen.add(matched_key)

    missing = [key for key in replacements if key not in seen]
    if missing:
        if updated and updated[-1].strip():
            updated.append("")
        updated.append("# Local PostgreSQL/pgvector database")
        updated.extend(replacements[key] for key in missing)

    return updated


def upsert_env_file(path: Path, database_url: str) -> None:
    lines = path.read_text(encoding="utf-8-sig").splitlines() if path.exists() else []
    updated = env_lines_with_database_url(lines, database_url)
    path.write_text("\n".join(updated) + "\n", encoding="utf-8")
    try:
        display_path = path.relative_to(BACKEND_ROOT)
    except ValueError:
        display_path = path
    print(f"{display_path}: DATABASE_URL updated")


def main() -> None:
    args = parse_args()
    source_path = resolve_backend_path(args.source)
    if not source_path.exists():
        raise SystemExit(
            f"SQLite source database was not found: {source_path}\n"
            "Place factory_space.db in backend/ or pass --source <path>.",
        )

    for env_path_text in args.write_env:
        upsert_env_file(resolve_backend_path(env_path_text), args.database_url)

    source_engine, source_tables = load_source_tables(f"sqlite:///{source_path}")
    target_engine = create_engine(args.database_url)
    target_tables_by_name = target_tables()

    with source_engine.connect() as source_connection, target_engine.begin() as target_connection:
        counts = read_counts(
            source_connection,
            target_connection,
            source_tables,
            target_tables_by_name,
        )
        if has_same_existing_data(counts):
            print("PostgreSQL already has matching row counts. Skipping copy.")
            return

        if has_different_existing_data(counts) and not args.reset:
            print("PostgreSQL has existing data with different row counts:")
            for table_name, (source_count, target_count) in counts.items():
                print(f"  {table_name}: sqlite={source_count}, postgres={target_count}")
            raise SystemExit("Set RESET_POSTGRES_DATA=1 to replace PostgreSQL data.")

        if args.reset:
            delete_target_rows(target_connection, target_tables_by_name)

        copy_rows(
            source_connection,
            target_connection,
            source_tables,
            target_tables_by_name,
        )
        reset_sequences(target_connection)

    print("SQLite data copy complete.")


if __name__ == "__main__":
    main()
