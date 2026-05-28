"""Agent runtime context."""

from __future__ import annotations

from datetime import UTC, datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class AgentContext(BaseModel):
    """Runtime context passed to every agent invocation."""

    model_config = ConfigDict(extra="forbid")

    session_id: str
    client_id: str | None = None
    user_id: str | None = None
    request_id: str | None = None
    world_state: dict[str, Any] = Field(default_factory=dict)
    agent_state: dict[str, Any] = Field(default_factory=dict)
    metadata: dict[str, Any] = Field(default_factory=dict)
    timestamp: datetime = Field(default_factory=lambda: datetime.now(UTC))
