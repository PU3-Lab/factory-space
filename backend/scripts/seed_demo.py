"""Seed a clean, reproducible demo database for the material_generation test console.

This loads the authored recipes (so item validation and recipe matching work) and
clears any previously generated experiments/materials so the test-console presets
return their intended fresh result_type instead of a cached one.

Run from the backend directory:

    DATABASE_URL="sqlite:///./factory_space.db" uv run python scripts/seed_demo.py

Covered result_type cases (exercise via /agent-test presets):
  - existing_recipe : Smelter [iron_ore x2]            -> matches Smelt_Iron
  - new_material    : Synthesizer [iron_ingot, copper_ingot] -> LLM proposal
  - cached_experiment: run the new_material preset twice
  - failed_result   : Smelter [iron_ingot, copper_ingot] (2 items in a Smelter)
  - failed_result   : Grinder [iron_ingot, copper_ingot] (intermediate mixed powder)
  - invalid_input   : Synthesizer [unobtanium] (unknown item)
"""

from __future__ import annotations

import sys
from pathlib import Path

from sqlalchemy import delete, select

backend_root = Path(__file__).resolve().parents[1]
if str(backend_root) not in sys.path:
    sys.path.insert(0, str(backend_root))

from db.engine import get_db_session  # noqa: E402
from db.models import (  # noqa: E402
    GeneratedExperimentModel,
    GeneratedMaterialDiscoveryModel,
    GeneratedMaterialModel,
    RecipeModel,
)
from scripts.ingest_recipes import ingest_recipes  # noqa: E402


def reset_generated_tables() -> None:
    """Clear all generated experiments, materials, and discoveries."""
    with get_db_session() as session:
        session.execute(delete(GeneratedMaterialDiscoveryModel))
        session.execute(delete(GeneratedExperimentModel))
        session.execute(delete(GeneratedMaterialModel))
    print("Cleared generated_experiments / generated_materials / discoveries.")


def main() -> None:
    ingest_recipes()
    reset_generated_tables()

    with get_db_session() as session:
        recipe_count = len(session.scalars(select(RecipeModel)).all())
    print(f"Demo seed complete. recipes={recipe_count}, generated tables empty.")


if __name__ == "__main__":
    main()
