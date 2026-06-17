"""Smoke test to run Alembic migrations on SQLite and verify table schemas."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path

from alembic import command
from alembic.config import Config
from sqlalchemy import create_engine, text

ROOT = Path(__file__).resolve().parent.parent


def test_alembic_migrations_sqlite_smoke() -> None:
    # 1. Create a temporary database file
    fd, temp_db_path = tempfile.mkstemp(suffix=".db")
    os.close(fd)

    db_url = f"sqlite:///{temp_db_path}"

    # Save the original DATABASE_URL env var if it exists
    orig_db_url = os.environ.get("DATABASE_URL")
    os.environ["DATABASE_URL"] = db_url

    try:
        # 2. Configure Alembic
        alembic_ini_path = ROOT / "alembic.ini"
        assert alembic_ini_path.exists(), f"alembic.ini not found at {alembic_ini_path}"

        config = Config(str(alembic_ini_path))

        # 3. Run upgrade head
        command.upgrade(config, "head")

        # 4. Connect and insert smoke data to verify tables
        engine = create_engine(db_url)
        with engine.begin() as conn:
            # Test recipes table
            conn.execute(
                text(
                    "INSERT INTO recipes (recipe_name, machine_type, crafting_time) "
                    "VALUES (:name, :machine, :time)"
                ),
                {"name": "test_recipe", "machine": "Synthesizer", "time": 2.0},
            )

            # Test manual_rag_documents table
            conn.execute(
                text(
                    "INSERT INTO manual_rag_documents (doc_id, source_file, source_row_id, title, content, content_hash, metadata_json, embedding, is_active) "
                    "VALUES (:doc_id, :src, :row, :title, :content, :hash, :meta, :embed, :active)"
                ),
                {
                    "doc_id": "doc_1",
                    "src": "test.csv",
                    "row": "1",
                    "title": "Test Doc",
                    "content": "Some content here",
                    "hash": "abc123hash",
                    "meta": "{}",
                    "embed": "[0.1, 0.2, 0.3]",  # Saved as JSON text on SQLite
                    "active": True,
                },
            )

            # Test generated_experiments table
            conn.execute(
                text(
                    "INSERT INTO generated_experiments (id, experiment_hash, machine_type, inputs_json, normalized_inputs_json, classification, result_type, recipe_name) "
                    "VALUES (:id, :hash, :machine, :inputs, :norm, :class, :res, :recipe)"
                ),
                {
                    "id": "exp_smoke_1",
                    "hash": "exphash123",
                    "machine": "Synthesizer",
                    "inputs": "[]",
                    "norm": "[]",
                    "class": "candidate",
                    "res": "new_material",
                    "recipe": "test_recipe",
                },
            )

            # Test generated_materials table
            conn.execute(
                text(
                    "INSERT INTO generated_materials (id, material_hash, name, category, rarity, properties_json) "
                    "VALUES (:id, :hash, :name, :category, :rarity, :props)"
                ),
                {
                    "id": "mat_smoke_1",
                    "hash": "mathash123",
                    "name": "Smoke Alloy",
                    "category": "alloy",
                    "rarity": "rare",
                    "props": "{}",
                },
            )

        # 5. Run downgrade base
        command.downgrade(config, "base")

    finally:
        # Clean up
        if orig_db_url is not None:
            os.environ["DATABASE_URL"] = orig_db_url
        else:
            os.environ.pop("DATABASE_URL", None)

        if 'engine' in locals():
            try:
                engine.dispose()
            except Exception:
                pass

        if os.path.exists(temp_db_path):
            os.remove(temp_db_path)
