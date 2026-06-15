"""Pytest conftest configuration for material generation agent tests."""

from __future__ import annotations

from collections.abc import Iterator

import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from agents.material_generation.events import MaterialEventPublisher
from agents.material_generation.recipe_repository import RecipeRepository
from agents.material_generation.visual_pipeline import VisualAssetPipeline
from db.models import (
    GeneratedExperimentModel,
    GeneratedMaterialDiscoveryModel,
    GeneratedMaterialModel,
    RecipeModel,
)


@pytest.fixture
def db_session() -> Iterator[Session]:
    """Provide an in-memory SQLite session prepopulated with basic test recipes."""
    MaterialEventPublisher.reset_executor(wait=False)
    engine = create_engine(
        "sqlite:///:memory:",
        echo=False,
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )

    # Create tables synchronously
    RecipeModel.__table__.create(engine)
    GeneratedExperimentModel.__table__.create(engine)
    GeneratedMaterialModel.__table__.create(engine)
    GeneratedMaterialDiscoveryModel.__table__.create(engine)

    session_factory = sessionmaker(bind=engine, expire_on_commit=False, class_=Session)

    VisualAssetPipeline.session_factory = session_factory

    session = session_factory()
    # Authored sample recipes matching actual data
    session.add(
        RecipeModel(
            recipe_name="Smelt_Iron",
            machine_type="Smelter",
            input_item_1="iron_ore",
            input_qty_1=2,
            output_item_1="iron_ingot",
            output_qty_1=1,
            crafting_time=3.0,
        )
    )
    session.add(
        RecipeModel(
            recipe_name="Crush_Iron",
            machine_type="Grinder",
            input_item_1="iron_ingot",
            input_qty_1=1,
            output_item_1="iron_powder",
            output_qty_1=2,
            crafting_time=1.5,
        )
    )
    session.commit()

    # Seed the recipe repository cache
    RecipeRepository.reload_cache(session)
    yield session

    session.close()

    # Wait for background jobs to finish within the testing session lifecycle
    MaterialEventPublisher.wait_for_jobs()
    VisualAssetPipeline.session_factory = None

    engine.dispose()
