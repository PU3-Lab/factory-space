"""Action schemas shared by backend agents and Unreal transport."""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class Action(BaseModel):
    """Structured command that Unreal can execute."""

    model_config = ConfigDict(extra="forbid")

    name: str = Field(min_length=1)
    args: dict[str, Any] = Field(default_factory=dict)


class ActionResult(BaseModel):
    """Execution result reported by Unreal for a previously requested action."""

    model_config = ConfigDict(extra="forbid")

    action: Action
    status: str = Field(min_length=1)
    result: dict[str, Any] = Field(default_factory=dict)
    error: dict[str, Any] | None = None
