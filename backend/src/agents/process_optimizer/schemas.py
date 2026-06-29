"""Process Optimizer가 주고받는 데이터 구조를 정의합니다.

Unreal 공장 snapshot, 분석 결과, 미리보기 계획, 성과 측정 응답을 Pydantic
모델로 검증하여 그래프 노드 사이의 데이터 계약을 일정하게 유지합니다.
"""

from __future__ import annotations

from datetime import datetime
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field


class InventoryItem(BaseModel):
    """장비의 입력 또는 출력 보관함에 있는 자원 한 종류를 표현합니다.

    자원 식별자와 현재 수량, 최대 보관량을 묶어 부족이나 포화 상태 계산에 사용합니다.
    """

    model_config = ConfigDict(extra="allow")

    item_id: str
    amount: float
    max_amount: float = 100.0


class MachineState(BaseModel):
    """공장 snapshot에 포함되는 장비 한 대의 현재 상태를 표현합니다.

    가동 상태, 가동률, 입출력 재고, 전력 소비량을 분석 도구에 전달합니다.
    """

    model_config = ConfigDict(extra="allow")

    id: str
    type: str = "unknown"
    status: str = "idle"  # e.g., "operating", "idle", "disabled"
    operating_rate: float = 0.0  # 0.0 to 1.0
    inputs: list[InventoryItem] = Field(default_factory=list)
    outputs: list[InventoryItem] = Field(default_factory=list)
    power_consumption: float = 0.0


class ConveyorState(BaseModel):
    """공장 snapshot에 포함되는 컨베이어 한 개의 상태를 표현합니다.

    컨베이어 식별자와 정체율을 보관하여 물류 병목 분석에 사용합니다.
    """

    model_config = ConfigDict(extra="allow")

    id: str
    congestion_rate: float = 0.0  # 0.0 to 1.0


class PowerGridState(BaseModel):
    """공장 전체 전력망의 생산량과 소비량을 표현합니다.

    분석 단계에서는 두 값을 비교하여 전력 부족 여부를 판단합니다.
    """

    model_config = ConfigDict(extra="allow")

    produced: float = 0.0
    consumed: float = 0.0


class FactoryState(BaseModel):
    """한 시점의 전체 공장 snapshot을 표현하는 최상위 모델입니다.

    장비, 컨베이어, 전력망 상태를 묶어 분석과 적용 전 충돌 검사에 전달합니다.
    """

    model_config = ConfigDict(extra="allow")

    machines: list[MachineState] = Field(default_factory=list)
    conveyors: list[ConveyorState] = Field(default_factory=list)
    power_grid: PowerGridState = Field(default_factory=PowerGridState)


class ProcessOptimizerPayload(BaseModel):
    """Process Optimizer 공개 요청의 기본 작업과 목표를 검증합니다.

    상태 업데이트, 분석, 적용, 되돌리기, 성과 측정 작업과 지원되는 최적화 목표만 허용합니다.
    """

    model_config = ConfigDict(extra="allow")

    operation: Literal["state_update", "analyze", "apply", "undo", "measure"] = (
        "analyze"
    )
    goal: Literal["balance", "throughput", "power_saving", "congestion_relief"] = (
        "balance"
    )
    request_source: str | None = None
    target: TargetDescriptor | None = None
    subquest_mode: bool | None = None


class TargetDescriptor(BaseModel):
    """최적화 제안이 가리키는 공장 구성 요소를 식별합니다.

    대상 종류와 Unreal에서 사용하는 고유 ID를 함께 보관합니다.
    """

    type: Literal["machine", "conveyor", "other"]
    id: str


class OptimizationSuggestion(BaseModel):
    """플레이어가 검토할 최적화 변경 제안 한 건을 표현합니다.

    문제, 권장 조치, 예상 효과, 위험도와 신뢰도를 포함하지만 실행 명령 자체는 담지 않습니다.
    """

    id: str
    target: TargetDescriptor | None = None
    problem: str
    recommended_action: str
    expected_effect: str
    risk: Literal["low", "medium", "high"] = "low"
    confidence: float = 1.0


class UiHints(BaseModel):
    """Unreal UI가 최적화 대상을 강조 표시할 때 사용하는 정보입니다.

    분석 결과와 관련된 장비 또는 컨베이어 식별자 목록을 전달합니다.
    """

    highlight_targets: list[str] = Field(default_factory=list)


class PowerSummary(BaseModel):
    """전력 분석 결과를 간단히 정리한 모델입니다.

    생산량과 소비량, 전력 부족 여부를 제안 생성 단계에 전달합니다.
    """

    model_config = ConfigDict(extra="allow")

    produced: float
    consumed: float
    power_issue: bool


class FactoryAnalysisReport(BaseModel):
    """공장 snapshot에서 결정론적으로 계산한 분석 결과입니다.

    평균 가동률과 병목 대상 목록, 컨베이어 정체, 전력 요약을 최적화 목표와 함께 제공합니다.
    """

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
    """플레이어 승인을 기다리는 최적화 미리보기 계획입니다.

    생성 당시의 세션, 공장 revision, 변경 제안, 예상 효과와 유효 기간을 저장합니다.
    """

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
    """최적화 적용 전후를 비교한 성과 측정 결과입니다.

    예상 효과와 실제 효과, 관찰 시간, 생산 주기를 바탕으로 성공 여부와 다음 행동을 전달합니다.
    """

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


class SuggestedSubquestNextRequest(BaseModel):
    """서브퀘스트 수락 시 다음에 보낼 요청의 정보입니다."""

    model_config = ConfigDict(extra="allow")

    agent: str = "process_optimizer"
    operation: str = "analyze"
    goal: str = "balance"
    request_source: str = "subquest"
    target: TargetDescriptor | None = None


class SuggestedSubquest(BaseModel):
    """플레이어에게 제안되는 서브퀘스트 상세 정보입니다."""

    model_config = ConfigDict(extra="allow")

    title: str
    objective: str
    target: TargetDescriptor | None = None
    severity: Literal["low", "medium", "high"] = "low"
    next_request: SuggestedSubquestNextRequest = Field(
        default_factory=SuggestedSubquestNextRequest
    )


class OptimizationAlert(BaseModel):
    """공장에 최적화가 필요할 때 생성되는 경고 및 서브퀘스트 추천 정보입니다."""

    needed: bool
    severity: Literal["low", "medium", "high"] = "low"
    reason: str = ""
    target: TargetDescriptor | None = None
    suggested_subquest: SuggestedSubquest | None = None


class ProcessOptimizerResponse(BaseModel):
    """Process Optimizer가 Unreal에 반환하는 통합 응답 모델입니다.

    작업 상태에 따라 미리보기, 승인된 변경, 실행 명령, 측정 결과와 오류 상태를 선택적으로 담습니다.
    """

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
    optimization_alert: OptimizationAlert | None = None
