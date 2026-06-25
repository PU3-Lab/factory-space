"""Pydantic schemas for process optimizer agent."""

from __future__ import annotations

from datetime import datetime
from typing import Literal, Any

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

    operation: Literal["state_update", "analyze", "apply", "undo", "measure"] = "analyze"
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


class PreviewPlan(BaseModel):
    """Schema representing a saved optimization preview plan."""

    plan_id: str
    session_id: str
    factoryRevision: int
    goal: str
    changes: list[OptimizationSuggestion] = Field(default_factory=list)
    expected_effect: dict[str, Any] = Field(default_factory=dict)
    ui_hints: UiHints = Field(default_factory=UiHints)
    created_at: datetime
    expires_at: datetime


class EffectMeasurementReport(BaseModel):
    """측정 성과 리포트 스키마."""

    status: Literal["success", "failed", "degraded"]
    """개선 성공, 미달(failed), 악화(degraded) 상태 분류"""

    next_action: Literal["monitor", "reanalyze"]
    """추천 차기 작업 (모니터링 유지 또는 최신 상태 재분석)"""

    expected_effect: dict[str, Any] = Field(default_factory=dict)
    """최적화 제안 당시 예상되었던 효과 지표"""

    actual_effect: dict[str, Any] = Field(default_factory=dict)
    """최적화 적용 후 실제 측정된 효과 지표"""

    observation_duration_seconds: float
    """적용 후 실시간 경과 관찰 시간(초 단위)"""

    production_cycles: int
    """적용 후 진행된 실제 생산 주기 수"""


class ProcessOptimizerResponse(BaseModel):
    """Response payload returned by the process optimizer agent."""

    status: Literal[
        "success",
        "suggestion",
        "preview",
        "apply_ready",
        "execute_ready",
        "undo_ready",
        "measurement_ready",
        "measurement_not_ready",
        "measurement_error",
        "plan_not_found",
        "plan_expired",
        "revision_conflict",
        "approval_required",
        "no_changes_selected",
        "invalid_change_id",
        "duplicate_execution",
        "invalid_command_payload",
        "record_not_found",
        "invalid_factory_state",
        "undo_conflict",
        "error",
    ] = "suggestion"
    factoryRevision: int  # noqa: N815 - Unreal WebSocket contract uses camelCase.
    goal: str = "balance"
    summary: str
    plan_id: str | None = None
    expires_at: str | None = None
    suggestions: list[OptimizationSuggestion] = Field(default_factory=list)
    changes: list[OptimizationSuggestion] = Field(default_factory=list)
    expected_effect: dict[str, Any] = Field(default_factory=dict)
    ui_hints: UiHints = Field(default_factory=UiHints)
    approved_changes: list[OptimizationSuggestion] = Field(default_factory=list)
    commands: list[dict[str, Any]] = Field(default_factory=list)
    measurement_result: EffectMeasurementReport | None = None
