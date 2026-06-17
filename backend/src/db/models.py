"""SQLAlchemy models for the Factory Space backend."""

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from agents.quest_generator.models import QuestInstance, QuestProgress

from sqlalchemy import (
    JSON,
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Index,
    Integer,
    String,
    Text,
    UniqueConstraint,
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
    state: Mapped[str] = mapped_column(
        String(20), default="solid", server_default="solid", nullable=False
    )

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


class QuestInstanceModel(Base):
    """Model representing a quest instance assigned to a factory."""

    __tablename__ = "quest_instances"
    __table_args__ = (
        Index("ix_quest_instances_factory_status", "factory_id", "status"),
    )

    id: Mapped[str] = mapped_column(String(100), primary_key=True)
    factory_id: Mapped[str] = mapped_column(String(100), nullable=False, index=True)
    quest_type: Mapped[str] = mapped_column(
        String(50), nullable=False
    )  # e.g., "support"
    support_type: Mapped[str] = mapped_column(
        String(50), nullable=False
    )  # e.g., "collect_item"
    related_main_quest_id: Mapped[str | None] = mapped_column(
        String(100), nullable=True
    )
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    description: Mapped[str] = mapped_column(Text, nullable=False)
    status: Mapped[str] = mapped_column(
        String(50),
        default="in_progress",
        server_default="in_progress",
        nullable=False,
    )
    objective_json: Mapped[list] = mapped_column(JSON, nullable=False)
    reward_json: Mapped[list] = mapped_column(JSON, nullable=False)
    created_by: Mapped[str] = mapped_column(
        String(50), default="rule", server_default="rule", nullable=False
    )
    level_reward_applied: Mapped[bool] = mapped_column(
        Boolean, default=False, server_default="false", nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        nullable=False,
    )
    completed_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=True), nullable=True
    )

    def to_pydantic(self) -> QuestInstance:
        """Converts the DB model to its corresponding Pydantic QuestInstance schema."""
        from agents.quest_generator.models import (
            QuestInstance,
            QuestObjective,
            QuestReward,
        )
        return QuestInstance(
            id=self.id,
            factory_id=self.factory_id,
            quest_type=self.quest_type,
            support_type=self.support_type,
            related_main_quest_id=self.related_main_quest_id,
            title=self.title,
            description=self.description,
            status=self.status,
            objectives=[QuestObjective.model_validate(obj) for obj in self.objective_json],
            rewards=[QuestReward.model_validate(r) for r in self.reward_json],
            created_by=self.created_by,
            level_reward_applied=self.level_reward_applied,
            created_at=self.created_at,
            completed_at=self.completed_at,
        )


class QuestProgressModel(Base):
    """Model representing progress of a specific quest objective."""

    __tablename__ = "quest_progress"
    __table_args__ = (
        UniqueConstraint(
            "quest_instance_id",
            "objective_id",
            name="uq_quest_progress_instance_objective",
        ),
    )

    id: Mapped[str] = mapped_column(String(100), primary_key=True)
    quest_instance_id: Mapped[str] = mapped_column(
        String(100),
        ForeignKey("quest_instances.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )
    objective_id: Mapped[str] = mapped_column(String(100), nullable=False)
    objective_type: Mapped[str] = mapped_column(String(50), nullable=False)
    target_id: Mapped[str] = mapped_column(String(100), nullable=False)
    current_amount: Mapped[int] = mapped_column(
        Integer, default=0, server_default="0", nullable=False
    )
    target_amount: Mapped[int] = mapped_column(Integer, nullable=False)
    status: Mapped[str] = mapped_column(
        String(50),
        default="in_progress",
        server_default="in_progress",
        nullable=False,
    )
    last_event_id: Mapped[str | None] = mapped_column(
        String(100), nullable=True, index=True
    )

    def to_pydantic(self) -> QuestProgress:
        """Converts the DB model to its corresponding Pydantic QuestProgress schema."""
        from agents.quest_generator.models import QuestProgress
        return QuestProgress(
            id=self.id,
            quest_instance_id=self.quest_instance_id,
            objective_id=self.objective_id,
            objective_type=self.objective_type,
            target_id=self.target_id,
            current_amount=self.current_amount,
            target_amount=self.target_amount,
            status=self.status,
            last_event_id=self.last_event_id,
        )
