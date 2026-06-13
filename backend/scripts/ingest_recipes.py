"""Ingest game recipes from CSV into the database (Synchronous)."""

from __future__ import annotations

import csv
import sys
from pathlib import Path

from sqlalchemy import select
from sqlalchemy.dialects.postgresql import insert as pg_insert

# Allow importing backend modules
backend_root = Path(__file__).resolve().parents[1]
if str(backend_root) not in sys.path:
    sys.path.insert(0, str(backend_root))

from db.engine import get_db_session  # noqa: E402
from db.models import RecipeModel  # noqa: E402

CSV_PATH = (
    backend_root.parent
    / "frontend"
    / "Source"
    / "Wanted_Factory"
    / "Data"
    / "RecipeTable.csv"
)


def _parse_int(value: str) -> int | None:
    if not value or not value.strip():
        return None
    try:
        return int(value.strip())
    except ValueError:
        return None


def _parse_float(value: str) -> float:
    if not value or not value.strip():
        return 1.0
    try:
        return float(value.strip())
    except ValueError:
        return 1.0


def ingest_recipes() -> None:
    """Read CSV, clean data, and perform synchronous upsert to the recipes table."""
    if not CSV_PATH.exists():
        print(f"Error: CSV file not found at {CSV_PATH}")
        return

    print(f"Reading recipes from {CSV_PATH}...")

    recipes_to_upsert = []
    with open(CSV_PATH, encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            recipe_name = row["RecipeName"].strip()
            machine_type = row["MachineType"].strip()
            if not recipe_name or not machine_type:
                continue

            recipes_to_upsert.append(
                {
                    "recipe_name": recipe_name,
                    "machine_type": machine_type,
                    "input_item_1": row.get("InputItem1", "").strip() or None,
                    "input_qty_1": _parse_int(row.get("InputQty1", "")),
                    "input_item_2": row.get("InputItem2", "").strip() or None,
                    "input_qty_2": _parse_int(row.get("InputQty2", "")),
                    "input_item_3": row.get("InputItem3", "").strip() or None,
                    "input_qty_3": _parse_int(row.get("InputQty3", "")),
                    "output_item_1": row.get("OutputItem1", "").strip() or None,
                    "output_qty_1": _parse_int(row.get("OutputQty1", "")),
                    "output_item_2": row.get("OutputItem2", "").strip() or None,
                    "output_qty_2": _parse_int(row.get("OutputQty2", "")),
                    "crafting_time": _parse_float(row.get("CraftingTime", "1.0")),
                }
            )

    if not recipes_to_upsert:
        print("No recipes found to ingest.")
        return

    with get_db_session() as session:
        # Check current count for logging
        count_before = len(session.scalars(select(RecipeModel)).all())
        print(f"Current recipe count in DB: {count_before}")

        dialect_name = session.bind.dialect.name
        if dialect_name == "postgresql":
            for r in recipes_to_upsert:
                stmt = pg_insert(RecipeModel).values(**r)
                stmt = stmt.on_conflict_do_update(
                    index_elements=[RecipeModel.recipe_name],
                    set_={
                        "machine_type": stmt.excluded.machine_type,
                        "input_item_1": stmt.excluded.input_item_1,
                        "input_qty_1": stmt.excluded.input_qty_1,
                        "input_item_2": stmt.excluded.input_item_2,
                        "input_qty_2": stmt.excluded.input_qty_2,
                        "input_item_3": stmt.excluded.input_item_3,
                        "input_qty_3": stmt.excluded.input_qty_3,
                        "output_item_1": stmt.excluded.output_item_1,
                        "output_qty_1": stmt.excluded.output_qty_1,
                        "output_item_2": stmt.excluded.output_item_2,
                        "output_qty_2": stmt.excluded.output_qty_2,
                        "crafting_time": stmt.excluded.crafting_time,
                    },
                )
                session.execute(stmt)
        else:
            # Fallback for SQLite
            for r in recipes_to_upsert:
                existing = session.scalars(
                    select(RecipeModel).where(
                        RecipeModel.recipe_name == r["recipe_name"]
                    )
                ).first()

                if existing:
                    for key, val in r.items():
                        setattr(existing, key, val)
                else:
                    session.add(RecipeModel(**r))

        print(f"Successfully upserted {len(recipes_to_upsert)} recipes.")


if __name__ == "__main__":
    ingest_recipes()
