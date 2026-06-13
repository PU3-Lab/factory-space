"""SQLAlchemy models for the Factory Space backend."""

from __future__ import annotations

from datetime import datetime

from sqlalchemy import (
    JSON,
    Boolean,
    DateTime,
    Float,
    Integer,
    String,
    Text,
    func,
)
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    """Shared database metadata base."""


class RecipeModel(Base):
    """Model representing an authored game recipe from RecipeTable.csv."""

    __tablename__ = "recipes"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    recipe_name: Mapped[str] = mapped_column(String(255), unique=True, nullable=False)
    machine_type: Mapped[str] = mapped_column(String(100), nullable=False)

    input_item_1: Mapped[str | None] = mapped_column(String(100), nullable=True)
    input_qty_1: Mapped[int | None] = mapped_column(Integer, nullable=True)
    input_item_2: Mapped[str | None] = mapped_column(String(100), nullable=True)
    input_qty_2: Mapped[int | None] = mapped_column(Integer, nullable=True)
    input_item_3: Mapped[str | None] = mapped_column(String(100), nullable=True)
    input_qty_3: Mapped[int | None] = mapped_column(Integer, nullable=True)

    output_item_1: Mapped[str | None] = mapped_column(String(100), nullable=True)
    output_qty_1: Mapped[int | None] = mapped_column(Integer, nullable=True)
    output_item_2: Mapped[str | None] = mapped_column(String(100), nullable=True)
    output_qty_2: Mapped[int | None] = mapped_column(Integer, nullable=True)

    crafting_time: Mapped[float] = mapped_column(Float, default=1.0)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
    )


class GeneratedExperimentModel(Base):
    """Model representing a synthesized experiment run and its outcome."""

    __tablename__ = "generated_experiments"

    id: Mapped[str] = mapped_column(String(100), primary_key=True)
    experiment_hash: Mapped[str] = mapped_column(
        String(64), unique=True, nullable=False
    )

    machine_type: Mapped[str] = mapped_column(String(100), nullable=False)
    inputs_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    normalized_inputs_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    process_conditions_json: Mapped[dict | None] = mapped_column(JSON, nullable=True)

    classification: Mapped[str] = mapped_column(String(50), nullable=False)
    result_type: Mapped[str] = mapped_column(String(50), nullable=False)

    recipe_name: Mapped[str | None] = mapped_column(String(255), nullable=True)
    material_id: Mapped[str | None] = mapped_column(String(100), nullable=True)
    output_items_json: Mapped[list | None] = mapped_column(JSON, nullable=True)
    failure_reason: Mapped[str | None] = mapped_column(Text, nullable=True)

    similar_experiments_json: Mapped[list | None] = mapped_column(JSON, nullable=True)

    llm_used: Mapped[bool] = mapped_column(
        Boolean, default=False, server_default="false"
    )
    llm_prompt_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    llm_model: Mapped[str | None] = mapped_column(String(100), nullable=True)
    llm_confidence: Mapped[float | None] = mapped_column(Float, nullable=True)

    created_by: Mapped[str | None] = mapped_column(String(100), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        onupdate=func.now(),
    )


class GeneratedMaterialModel(Base):
    """Model representing an unlocked/generated new material and visual asset status."""

    __tablename__ = "generated_materials"

    id: Mapped[str] = mapped_column(String(100), primary_key=True)
    material_hash: Mapped[str] = mapped_column(String(64), unique=True, nullable=False)

    name: Mapped[str] = mapped_column(String(100), nullable=False)
    category: Mapped[str] = mapped_column(String(50), nullable=False)
    rarity: Mapped[str] = mapped_column(String(50), nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)

    properties_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    risks_json: Mapped[list | None] = mapped_column(JSON, nullable=True)
    usage_json: Mapped[list | None] = mapped_column(JSON, nullable=True)
    recipe_candidates_json: Mapped[list | None] = mapped_column(JSON, nullable=True)

    source_experiment_hash: Mapped[str | None] = mapped_column(
        String(64), nullable=True
    )
    generation_status: Mapped[str] = mapped_column(
        String(50), default="created", server_default="created"
    )

    visual_status: Mapped[str] = mapped_column(
        String(50), default="pending", server_default="pending"
    )
    visual_prompt: Mapped[str | None] = mapped_column(Text, nullable=True)
    visual_asset_key: Mapped[str | None] = mapped_column(String(255), nullable=True)
    texture_asset_key: Mapped[str | None] = mapped_column(String(255), nullable=True)
    thumbnail_asset_key: Mapped[str | None] = mapped_column(String(255), nullable=True)
    fallback_icon: Mapped[str | None] = mapped_column(String(255), nullable=True)
    visual_error: Mapped[str | None] = mapped_column(Text, nullable=True)

    balance_score: Mapped[float | None] = mapped_column(Float, nullable=True)
    is_approved: Mapped[bool] = mapped_column(
        Boolean, default=False, server_default="false"
    )

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        onupdate=func.now(),
    )


class GeneratedMaterialDiscoveryModel(Base):
    """Model representing a player's first-time discovery of a generated material."""

    __tablename__ = "generated_material_discoveries"

    id: Mapped[str] = mapped_column(String(100), primary_key=True)
    material_id: Mapped[str] = mapped_column(String(100), nullable=False)
    player_id: Mapped[str] = mapped_column(String(100), nullable=False)
    experiment_hash: Mapped[str] = mapped_column(String(64), nullable=False)

    discovered_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
    )
