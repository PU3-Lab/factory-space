"""Pydantic schemas for process optimizer agent."""

from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field


class InventoryItem(BaseModel):
    """Schema for inventory items inside machine inputs/outputs."""

    model_config = ConfigDict(extra="allow")

    item_id: str
    amount: float
    max_amount: float = 100.0


class MachineState(BaseModel):
    """Schema for individual machine states in the factory."""

    model_config = ConfigDict(extra="allow")

    id: str
    type: str = "unknown"
    status: str = "idle"  # e.g., "operating", "idle", "disabled"
    operating_rate: float = 0.0  # 0.0 to 1.0
    inputs: list[InventoryItem] = Field(default_factory=list)
    outputs: list[InventoryItem] = Field(default_factory=list)
    power_consumption: float = 0.0


class ConveyorState(BaseModel):
    """Schema for individual conveyor states in the factory."""

    model_config = ConfigDict(extra="allow")

    id: str
    congestion_rate: float = 0.0  # 0.0 to 1.0


class PowerGridState(BaseModel):
    """Schema for the overall power grid state."""

    model_config = ConfigDict(extra="allow")

    produced: float = 0.0
    consumed: float = 0.0


class FactoryState(BaseModel):
    """Schema representing the overall state of the factory."""

    model_config = ConfigDict(extra="allow")

    machines: list[MachineState] = Field(default_factory=list)
    conveyors: list[ConveyorState] = Field(default_factory=list)
    power_grid: PowerGridState = Field(default_factory=PowerGridState)


class ProcessOptimizerPayload(BaseModel):
    """Schema for process optimizer payload validation."""

    model_config = ConfigDict(extra="allow")

    operation: Literal["state_update", "analyze"] = "analyze"
    goal: Literal["balance", "throughput", "power_saving", "congestion_relief"] = (
        "balance"
    )


class TargetDescriptor(BaseModel):
    """Descriptor pointing to a specific machine or component in the factory."""

    type: Literal["machine", "conveyor", "other"]
    id: str


class OptimizationSuggestion(BaseModel):
    """Schema for a single optimization suggestion."""

    id: str
    target: TargetDescriptor | None = None
    problem: str
    recommended_action: str
    expected_effect: str
    risk: Literal["low", "medium", "high"] = "low"
    confidence: float = 1.0


class UiHints(BaseModel):
    """Hints for Unreal UI to highlight specific targets."""

    highlight_targets: list[str] = Field(default_factory=list)


class PowerSummary(BaseModel):
    """Summary of the power grid status."""

    model_config = ConfigDict(extra="allow")

    produced: float
    consumed: float
    power_issue: bool


class FactoryAnalysisReport(BaseModel):
    """Analysis results containing calculated metrics and issues."""

    model_config = ConfigDict(extra="allow")

    factoryRevision: int  # noqa: N815 - Unreal WebSocket contract uses camelCase.
    goal: str
    average_operating_rate: float
    input_shortages: list[str] = Field(default_factory=list)  # Machine IDs
    output_blocked: list[str] = Field(default_factory=list)  # Machine IDs
    congested_conveyors: list[str] = Field(default_factory=list)  # Conveyor IDs
    average_conveyor_congestion: float = 0.0
    power_summary: PowerSummary


class ProcessOptimizerResponse(BaseModel):
    """Response payload returned by the process optimizer agent."""

    status: Literal["success", "suggestion", "error"] = "suggestion"
    factoryRevision: int  # noqa: N815 - Unreal WebSocket contract uses camelCase.
    goal: str = "balance"
    summary: str
    suggestions: list[OptimizationSuggestion] = Field(default_factory=list)
    ui_hints: UiHints = Field(default_factory=UiHints)
