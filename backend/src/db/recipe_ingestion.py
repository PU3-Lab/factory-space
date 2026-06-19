"""CSV 레시피 데이터를 recipes 테이블에 적재하는 공용 유틸리티입니다."""

from __future__ import annotations

import csv
from pathlib import Path

from sqlalchemy import select
from sqlalchemy.dialects.postgresql import insert as pg_insert

from db.engine import get_db_session
from db.models import RecipeModel

BACKEND_ROOT = Path(__file__).resolve().parents[2]
CSV_PATH = (
    BACKEND_ROOT.parent
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
    """CSV 레시피를 읽어서 recipes 테이블에 upsert합니다."""
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
        count_before = len(session.scalars(select(RecipeModel)).all())
        print(f"Current recipe count in DB: {count_before}")

        dialect_name = session.bind.dialect.name
        if dialect_name == "postgresql":
            for recipe in recipes_to_upsert:
                stmt = pg_insert(RecipeModel).values(**recipe)
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
            for recipe in recipes_to_upsert:
                existing = session.scalars(
                    select(RecipeModel).where(
                        RecipeModel.recipe_name == recipe["recipe_name"]
                    )
                ).first()

                if existing:
                    for key, value in recipe.items():
                        setattr(existing, key, value)
                else:
                    session.add(RecipeModel(**recipe))

        print(f"Successfully upserted {len(recipes_to_upsert)} recipes.")
