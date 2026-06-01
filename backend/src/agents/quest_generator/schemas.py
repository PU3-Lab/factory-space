"""Pydantic schemas for quest generation."""

from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, Field


class QuestObjective(BaseModel):
    """Single measurable quest objective."""

    action: Literal["mine", "inspect"]
    target_item_id: str = Field(min_length=1)
    target_item_name: str = Field(min_length=1)
    quantity: int = Field(gt=0)


class Quest(BaseModel):
    """Validated quest payload returned to the client."""

    id: str = Field(min_length=1)
    type: Literal["production", "tutorial", "exploration", "economy"]
    title: str = Field(min_length=1)
    description: str = Field(min_length=1)
    objectives: list[QuestObjective] = Field(min_length=1)


class QuestResponse(BaseModel):
    """Top-level quest response payload."""

    quest: Quest
