"""Process Optimizer LangGraph shared state.

LangGraph node들이 서로 넘겨받는 공통 상태 타입을 정의한다.
"""

from datetime import datetime
from typing import Any, Optional, TypedDict

from agents.process_optimizer.schemas import (
    FactoryAnalysisReport,
    OptimizationSuggestion,
    PreviewPlan,
    UiHints,
)


class ProcessOptimizerGraphState(TypedDict):
    """Process Optimizer 전용 그래프에서 사용하는 공유 상태."""

    payload: dict[str, Any]
    """WebSocket 요청 payload 원본."""

    context: Optional[dict[str, Any]]
    """WebSocket envelope context. Unreal may send factoryRevision here instead of payload."""

    session_id: Optional[str]
    """플레이어 세션 ID."""

    operation: str
    """실행할 작업 모드: analyze, apply, undo, measure."""

    factory_state: Optional[dict[str, Any]]
    """Unreal에서 받은 공장 상태 snapshot."""

    before_states: Optional[Any]
    """Apply 승인 직전 Unreal이 보낸 변경 항목별 before snapshot."""

    after_states: Optional[Any]
    """Apply 실행 직후 Unreal이 보낸 변경 항목별 after snapshot. 없으면 planned command만 저장."""

    factoryRevision: int
    """Unreal이 관리하는 공장 상태 버전 번호."""

    goal: str
    """최적화 목표: balance, throughput, power_saving, congestion_relief."""

    metrics: Optional[FactoryAnalysisReport]
    """코드로 계산한 공장 분석 지표."""

    suggestions: Optional[list[OptimizationSuggestion]]
    """분석 결과에서 만든 최적화 제안 목록."""

    ui_hints: Optional[UiHints]
    """Unreal UI 하이라이트 정보."""

    plan_id: Optional[str]
    """저장된 preview plan ID."""

    expires_at: Optional[datetime]
    """preview plan 만료 시각."""

    preview_plan: Optional[PreviewPlan]
    """저장소에서 조회하거나 새로 만든 preview plan."""

    approved_change_ids: Optional[list[str]]
    """플레이어가 승인한 change ID 목록."""

    approved_changes: Optional[list[OptimizationSuggestion]]
    """검증을 통과한 승인 대상 제안 목록."""

    commands: Optional[list[dict[str, Any]]]
    """Unreal에 보낼 검증된 명령 payload 목록."""

    execution_records: Optional[list[Any]]
    """선택 적용/되돌리기에 사용할 실행 기록 목록."""

    previewPayload: Optional[dict[str, Any]]
    """WebSocket 응답 payload."""

    error: Optional[str]
    """처리 중 발생한 오류 메시지."""

    error_type: Optional[str]
    """오류 유형: plan_not_found, plan_expired, revision_conflict 등."""

    conflicts: Optional[list[str]]
    """되돌리기(Undo) 시 충돌이 발생한 change_id 목록."""

    before_metrics: Optional[Any]
    """최적화 적용 전 복구된 가상 상태의 공장 분석 지표."""

    measurement_result_data: Optional[dict[str, Any]]
    """예상치와 실측치를 대조한 측정 결과 중간 데이터."""

    measurement_result: Optional[Any]
    """최종 판정 및 등급이 수집된 측정 성과 리포트."""
