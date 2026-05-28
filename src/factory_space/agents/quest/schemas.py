"""Quest schemas."""

from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator

from factory_space.core.actions.schemas import Action

QuestPriority = Literal["low", "medium", "high"]


class QuestWorldObject(BaseModel):
    """Object data used by the mock quest generator."""

    model_config = ConfigDict(extra="forbid")

    id: str = Field(min_length=1)
    type: str = "object"
    status: str = "unknown"


class QuestMachine(BaseModel):
    """Machine data used to detect simple bottleneck quests."""

    model_config = ConfigDict(extra="forbid")

    id: str = Field(min_length=1)
    input_rate: float = Field(default=0, ge=0)
    output_rate: float = Field(default=0, ge=0)
    status: str = "unknown"

    @property
    def throughput_ratio(self) -> float:
        """Return output/input ratio, treating missing input as healthy."""

        if self.input_rate == 0:
            return 1.0
        return self.output_rate / self.input_rate


class QuestGameState(BaseModel):
    """Mock game state accepted by the quest service."""

    model_config = ConfigDict(extra="forbid")

    player_location: str = "unknown"
    nearby_objects: list[QuestWorldObject] = Field(default_factory=list)
    machines: list[QuestMachine] = Field(default_factory=list)

    @field_validator("nearby_objects", mode="before")
    @classmethod
    def normalize_nearby_objects(cls, value: object) -> object:
        """Allow simple object-id strings in smoke scenarios."""

        if isinstance(value, list):
            return [{"id": item} if isinstance(item, str) else item for item in value]
        return value


class QuestGenerationRequest(BaseModel):
    """Payload accepted by the quest agent service."""

    model_config = ConfigDict(extra="forbid")

    event: str = "state_update"
    object_id: str | None = None
    quest_id: str | None = None
    query: str | None = None
    game_state: QuestGameState = Field(default_factory=QuestGameState)

    @model_validator(mode="before")
    @classmethod
    def accept_world_state_alias(cls, data: object) -> object:
        """Support the protocol docs' `world_state` example as an alias."""

        if isinstance(data, dict) and "game_state" not in data and "world_state" in data:
            return {**data, "game_state": data["world_state"]}
        return data


class QuestObjective(BaseModel):
    """Single objective for a generated quest."""

    model_config = ConfigDict(extra="forbid")

    description: str = Field(min_length=1)
    target_object_id: str = Field(min_length=1)
    required_event: str = Field(min_length=1)
    completed: bool = False


class Quest(BaseModel):
    """Generated quest payload."""

    model_config = ConfigDict(extra="forbid")

    quest_id: str = Field(min_length=1)
    title: str = Field(min_length=1)
    description: str = Field(min_length=1)
    priority: QuestPriority
    objective: QuestObjective


class QuestGenerationResult(BaseModel):
    """Validated service output returned as JSON."""

    model_config = ConfigDict(extra="forbid")

    text: str = Field(min_length=1)
    quest: Quest
    actions: list[Action] = Field(min_length=1)
    metadata: dict[str, Any] = Field(default_factory=dict)
