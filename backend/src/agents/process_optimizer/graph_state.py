"""Process Optimizer LangGraph 노드가 공유하는 상태 타입을 정의합니다.

각 노드는 이 상태에서 필요한 입력을 읽고 새로운 분석 결과, 실행 명령,
오류 정보를 추가하여 다음 노드로 전달합니다.
"""

from datetime import datetime
from typing import Any, TypedDict

from agents.process_optimizer.schemas import (
    FactoryAnalysisReport,
    OptimizationSuggestion,
    PreviewPlan,
    UiHints,
)


class ProcessOptimizerGraphState(TypedDict):
    """Process Optimizer 전체 실행 단계가 함께 사용하는 상태 딕셔너리입니다.

    WebSocket 입력부터 분석 지표, 승인된 변경, Unreal 명령, 측정 결과까지
    한 요청을 처리하는 동안 노드 사이에서 전달되는 값을 정의합니다.
    """

    payload: dict[str, Any]
    """WebSocket 요청 payload 원본."""

    context: dict[str, Any] | None
    """WebSocket envelope의 context이며 revision이 이 위치로 전달될 수도 있습니다."""

    session_id: str | None
    """플레이어 세션 ID."""

    operation: str
    """실행할 작업 모드: analyze, apply, undo, measure."""

    factory_state: dict[str, Any] | None
    """Unreal에서 받은 공장 상태 snapshot."""

    before_states: Any | None
    """Apply 승인 직전 Unreal이 보낸 변경 항목별 before snapshot."""

    after_states: Any | None
    """적용 직후 Unreal이 보낸 변경 항목별 상태이며, 없으면 예정 명령만 저장합니다."""

    factoryRevision: int
    """Unreal이 관리하는 공장 상태 버전 번호."""

    goal: str
    """최적화 목표: balance, throughput, power_saving, congestion_relief."""

    metrics: FactoryAnalysisReport | None
    """코드로 계산한 공장 분석 지표."""

    suggestions: list[OptimizationSuggestion] | None
    """분석 결과에서 만든 최적화 제안 목록."""

    ui_hints: UiHints | None
    """Unreal UI 하이라이트 정보."""

    plan_id: str | None
    """저장된 preview plan ID."""

    expires_at: datetime | None
    """preview plan 만료 시각."""

    preview_plan: PreviewPlan | None
    """저장소에서 조회하거나 새로 만든 preview plan."""

    approved_change_ids: list[str] | None
    """플레이어가 승인한 change ID 목록."""

    approved_changes: list[OptimizationSuggestion] | None
    """검증을 통과한 승인 대상 제안 목록."""

    commands: list[dict[str, Any]] | None
    """Unreal에 보낼 검증된 명령 payload 목록."""

    execution_records: list[Any] | None
    """선택 적용/되돌리기에 사용할 실행 기록 목록."""

    previewPayload: dict[str, Any] | None
    """WebSocket 응답 payload."""

    error: str | None
    """처리 중 발생한 오류 메시지."""

    error_type: str | None
    """오류 유형: plan_not_found, plan_expired, revision_conflict 등."""

    conflicts: list[str] | None
    """되돌리기(Undo) 시 충돌이 발생한 change_id 목록."""

    before_metrics: Any | None
    """최적화 적용 전 복구된 가상 상태의 공장 분석 지표."""

    measurement_result_data: dict[str, Any] | None
    """예상치와 실측치를 대조한 측정 결과 중간 데이터."""

    measurement_result: Any | None
    """최종 판정 및 등급이 수집된 측정 성과 리포트."""
