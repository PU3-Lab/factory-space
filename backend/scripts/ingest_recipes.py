"""Ingest game recipes from CSV into the database."""

from __future__ import annotations

import sys
from pathlib import Path

backend_root = Path(__file__).resolve().parents[1]
src_root = backend_root / "src"
if str(src_root) not in sys.path:
    sys.path.insert(0, str(src_root))

from db.recipe_ingestion import ingest_recipes  # noqa: E402

if __name__ == "__main__":
    ingest_recipes()
