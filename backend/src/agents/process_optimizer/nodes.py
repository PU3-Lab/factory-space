"""Process Optimizer 전용 LangGraph 노드를 정의합니다.

각 노드는 공통 ``ProcessOptimizerGraphState``에서 필요한 값을 읽고,
다음 단계에서 사용할 변경 필드만 딕셔너리로 반환합니다.
"""

import uuid
from datetime import UTC, datetime, timedelta
from typing import Any

from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
from agents.process_optimizer.commands import (
    build_command_payload,
    validate_command_payload,
)
from agents.process_optimizer.effect_measurement import (
    check_observation_window,
    check_production_cycles,
    evaluate_effects,
    recreate_before_state,
)
from agents.process_optimizer.execution_record import (
    ExecutionRecord,
    execution_record_store,
)
from agents.process_optimizer.graph_state import ProcessOptimizerGraphState
from agents.process_optimizer.preview_store import preview_plan_store
from agents.process_optimizer.schemas import (
    EffectMeasurementReport,
    FactoryState,
    PreviewPlan,
    UiHints,
)
from agents.process_optimizer.suggestion import (
    OptimizationSuggestionTool,
    SuggestionValidationTool,
)
from agents.process_optimizer.undo import (
    build_inverse_command,
    check_undo_conflict,
    get_component_state,
)

# ==========================================
# 0. Common routing nodes
# ==========================================


def route_operation(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """요청한 작업 종류를 읽어 그래프 분기에 사용할 값으로 정규화합니다.

    Args:
        state: 요청 payload를 포함한 그래프 공유 상태입니다.

    Returns:
        분석, 적용, 되돌리기, 측정 중 선택된 ``operation`` 값입니다.
    """
    payload = state.get("payload", {})
    operation = payload.get("operation") or "analyze"
    return {"operation": operation}


# ==========================================
# 1. Analyze flow nodes
# ==========================================


def validate_factory_state(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """분석 요청의 공장 상태를 검증하고 공통 입력값을 초기화합니다.

    Args:
        state: payload와 선택적인 WebSocket context를 포함한 공유 상태입니다.

    Returns:
        검증된 공장 상태, revision, 목표 또는 구조화된 오류 정보입니다.
    """
    payload = state.get("payload", {})
    context = state.get("context", {}) or {}
    operation = payload.get("operation") or "analyze"
    goal = payload.get("goal") or "balance"

    session_id = (
        state.get("session_id")
        or payload.get("session_id")
        or payload.get("session-id")
        or "default-session"
    )

    factory_state = payload.get("factory_state")
    revision = payload.get("factoryRevision")
    if revision is None:
        revision = context.get("factoryRevision")

    if not factory_state and payload and "machines" in payload:
        factory_state = payload

    if not factory_state:
        from agents.process_optimizer.snapshot_store import process_optimizer_snapshot_store
        client_id = context.get("client_id") or "unreal"
        snapshot = process_optimizer_snapshot_store.get_latest(session_id, client_id)
        if snapshot and snapshot.factory_state:
            factory_state = snapshot.factory_state
            if revision is None or revision == 0:
                revision = snapshot.factoryRevision

    error = None
    if not factory_state:
        error = "Factory state is missing in payload."
    else:
        try:
            if isinstance(factory_state, dict):
                state_obj = FactoryState.model_validate(factory_state)
            else:
                state_obj = factory_state

            if not state_obj.machines and not state_obj.conveyors:
                error = "Factory state contains no machines or conveyors."
        except Exception as e:
            error = f"Invalid factory state format: {str(e)}"

    if revision is None:
        revision = 0

    return {
        "operation": operation,
        "factory_state": factory_state,
        "factoryRevision": revision,
        "goal": goal,
        "session_id": session_id,
        "error": error,
    }


def calculate_metrics(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """검증된 공장 상태로부터 결정론적 분석 지표를 계산합니다.

    Args:
        state: 공장 snapshot과 최적화 목표가 준비된 공유 상태입니다.

    Returns:
        제안 생성에 사용할 ``FactoryAnalysisReport``를 담은 상태 변경값입니다.
    """
    if state.get("error"):
        return {}

    analyzer = FactoryStateAnalyzerTool()
    try:
        report = analyzer.analyze(
            factory_state=state["factory_state"],
            factory_revision=state["factoryRevision"],
            goal=state["goal"],
        )
        res = {"metrics": report}
        if report.need_more_state:
            res["need_more_state_payload"] = report.need_more_state
        return res
    except Exception as e:
        return {"error": f"Failed to calculate metrics: {str(e)}"}


def detect_bottlenecks(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """분석 지표에서 입력 부족, 출력 적체 등의 병목 대상을 정리합니다.

    Args:
        state: 계산된 공장 분석 지표를 포함한 공유 상태입니다.

    Returns:
        병목 종류별 대상 ID를 담은 딕셔너리입니다.
    """
    if state.get("error") or not state.get("metrics"):
        return {}

    metrics = state["metrics"]

    bottlenecks = {
        "input_shortages": metrics.input_shortages,
        "output_blocked": metrics.output_blocked,
        "congested_conveyors": metrics.congested_conveyors,
        "power_issue": metrics.power_summary.power_issue,
    }

    return {"bottlenecks": bottlenecks}


def build_optimization_candidates(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """분석 리포트를 기반으로 플레이어가 검토할 제안 후보를 만듭니다.

    Args:
        state: 분석 지표가 저장된 그래프 공유 상태입니다.

    Returns:
        최대 3개의 최적화 제안과 Unreal UI 하이라이트 정보입니다.
    """
    if state.get("error") or not state.get("metrics"):
        return {}

    metrics = state["metrics"]
    suggestion_tool = OptimizationSuggestionTool()
    suggestions, ui_hints = suggestion_tool.generate_suggestions(metrics)

    # Sprint 2: target 정보가 존재하는 경우 우선순위 조정 및 UI 하이라이트 추가
    payload = state.get("payload", {})
    target = payload.get("target")
    target_id = None
    if target:
        if isinstance(target, dict):
            target_id = target.get("id")
        else:
            target_id = getattr(target, "id", None)

    if target_id:
        # 1. target.id와 직접 일치하는 suggestion을 최상단으로 정렬
        if suggestions:
            matched = []
            others = []
            for sug in suggestions:
                if sug.target and sug.target.id == target_id:
                    matched.append(sug)
                else:
                    others.append(sug)
            suggestions = matched + others

        # 2. ui_hints.highlight_targets 보강
        if ui_hints is None:
            ui_hints = UiHints(highlight_targets=[])
        if target_id not in ui_hints.highlight_targets:
            ui_hints.highlight_targets.insert(0, target_id)

    return {
        "suggestions": suggestions,
        "ui_hints": ui_hints,
    }


def validate_preview_candidates(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """제안 후보가 개수 제한과 실행 안전 규칙을 지키는지 검사합니다.

    Args:
        state: 생성된 최적화 제안 목록을 포함한 공유 상태입니다.

    Returns:
        검증에 실패하면 오류 정보를, 통과하면 빈 변경값을 반환합니다.
    """
    if state.get("error") or not state.get("suggestions"):
        return {}

    validator = SuggestionValidationTool()
    is_valid = validator.validate_suggestions(state["suggestions"])

    if not is_valid:
        return {
            "error": "Optimization suggestions failed safety and business rule validation."
        }

    return {}


def create_preview_plan(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """검증된 제안을 플레이어 승인 대기용 미리보기 계획으로 묶습니다.

    Args:
        state: 제안, 세션, 공장 revision과 목표를 포함한 공유 상태입니다.

    Returns:
        새 계획 ID와 만료 시각이 포함된 ``PreviewPlan``입니다.
    """
    if state.get("error") or not state.get("suggestions"):
        return {}

    suggestions = state["suggestions"]
    ui_hints = state.get("ui_hints") or UiHints()

    plan_id = f"plan-{uuid.uuid4().hex[:8]}"
    created_at = datetime.now(UTC)
    expires_at = created_at + timedelta(minutes=5)

    expected_effect = {
        "estimated": False,
        "resolved_input_shortages_count": 0,
        "resolved_output_blocks_count": 0,
        "resolved_conveyor_congestions_count": 0,
    }

    for sug in suggestions:
        if "input" in sug.id:
            expected_effect["resolved_input_shortages_count"] += 1
        elif "output" in sug.id:
            expected_effect["resolved_output_blocks_count"] += 1
        elif "conveyor" in sug.id:
            expected_effect["resolved_conveyor_congestions_count"] += 1

    preview_plan = PreviewPlan(
        plan_id=plan_id,
        session_id=state.get("session_id") or "default-session",
        factoryRevision=state["factoryRevision"],
        goal=state["goal"],
        changes=suggestions,
        expected_effect=expected_effect,
        ui_hints=ui_hints,
        created_at=created_at,
        expires_at=expires_at,
    )

    return {
        "plan_id": plan_id,
        "expires_at": expires_at,
        "preview_plan": preview_plan,
    }


def save_preview_plan(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """생성된 미리보기 계획을 메모리 저장소에 보관합니다.

    Args:
        state: 저장할 ``PreviewPlan``을 포함한 공유 상태입니다.

    Returns:
        추가로 병합할 값이 없으므로 빈 딕셔너리를 반환합니다.
    """
    if state.get("error") or not state.get("preview_plan"):
        return {}

    preview_plan = state["preview_plan"]
    preview_plan_store.save(preview_plan)
    return {}


def return_preview_plan(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """분석 결과를 Unreal에 반환할 최종 미리보기 응답으로 구성합니다.

    Args:
        state: 계획, 제안, 예상 효과와 오류 정보를 포함한 공유 상태입니다.

    Returns:
        WebSocket 응답에 사용할 ``previewPayload`` 상태 변경값입니다.
    """
    if state.get("need_more_state_payload"):
        payload = dict(state["need_more_state_payload"])
        payload["factoryRevision"] = state.get("factoryRevision", 0)
        payload["goal"] = state.get("goal", "balance")
        return {"previewPayload": payload}

    if state.get("error"):
        error_payload = {
            "status": "error",
            "factoryRevision": state.get("factoryRevision", 0),
            "goal": state.get("goal", "balance"),
            "summary": f"최적화 미리보기 계획 생성 중 오류가 발생했습니다: {state['error']}",
            "changes": [],
            "suggestions": [],
            "expected_effect": {},
            "ui_hints": {"highlight_targets": []},
            "expires_at": None,
        }
        return {"previewPayload": error_payload}

    preview_plan = state.get("preview_plan")
    if not preview_plan:
        return {}

    suggestions = preview_plan.changes

    # Sprint 2: target 정보 기반 동적 summary 빌드
    payload = state.get("payload", {})
    target = payload.get("target")
    target_id = None
    if target:
        if isinstance(target, dict):
            target_id = target.get("id")
        else:
            target_id = getattr(target, "id", None)

    if target_id:
        # target 관련 매칭되는 제안이 첫 번째 자리에 위치하는지 확인 (이미 reorder되었기 때문)
        has_matching_suggestion = (
            len(suggestions) > 0
            and suggestions[0].target is not None
            and suggestions[0].target.id == target_id
        )
        if has_matching_suggestion:
            first_sug = suggestions[0]
            issue_desc = "공정상 문제"
            if "input" in first_sug.id:
                issue_desc = "입력 재고 부족"
            elif "output" in first_sug.id:
                issue_desc = "생산품 출력 적체"
            elif "conveyor" in first_sug.id:
                issue_desc = "컨베이어 이송 혼잡"

            summary_text = f"{target_id}을(를) 기준으로 확인한 결과, {issue_desc}이(가) 가장 먼저 해결할 문제입니다."
        else:
            summary_text = f"{target_id} 자체의 직접 병목은 크지 않지만, 전체 공장 기준의 최적화 개선안을 제안합니다."
    else:
        summary_text = "공장 상태 분석 결과에 따른 기본 추천 변경 계획입니다."
        if suggestions:
            summary_text = f"현재 공장의 병목과 비효율 문제를 해결하기 위해 {len(suggestions)}개의 최적화 개선안을 제안합니다."

    preview_payload = {
        "status": "preview",
        "plan_id": preview_plan.plan_id,
        "factoryRevision": preview_plan.factoryRevision,
        "goal": preview_plan.goal,
        "summary": summary_text,
        "changes": [s.model_dump() for s in suggestions],
        "suggestions": [s.model_dump() for s in suggestions],
        "expected_effect": preview_plan.expected_effect,
        "ui_hints": preview_plan.ui_hints.model_dump()
        if preview_plan.ui_hints
        else {"highlight_targets": []},
        "expires_at": preview_plan.expires_at.isoformat(),
    }

    return {"previewPayload": preview_payload}


# ==========================================
# 2. Apply flow nodes
# ==========================================


def validate_apply_request(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """적용 요청에 계획 ID와 공장 상태 등 필수 값이 있는지 검사합니다.

    Args:
        state: 플레이어의 적용 요청 payload를 포함한 공유 상태입니다.

    Returns:
        정규화된 적용 입력값 또는 누락된 필드에 대한 오류 정보입니다.
    """
    payload = state.get("payload", {})
    context = state.get("context", {}) or {}

    plan_id = payload.get("plan_id")
    approved_change_ids = payload.get("approved_change_ids")
    approval = payload.get("approval")
    factory_state = payload.get("factory_state")
    before_states = payload.get("before_states")
    after_states = payload.get("after_states")
    revision = payload.get("factoryRevision")
    if revision is None:
        revision = context.get("factoryRevision")

    session_id = (
        state.get("session_id")
        or payload.get("session_id")
        or payload.get("session-id")
        or "default-session"
    )

    if revision is None:
        revision = 0

    error = None
    error_type = None

    if not plan_id:
        error = "Required parameter 'plan_id' is missing."
        error_type = "plan_not_found"

    return {
        "plan_id": plan_id,
        "approved_change_ids": approved_change_ids,
        "factory_state": factory_state,
        "before_states": before_states,
        "after_states": after_states,
        "factoryRevision": revision,
        "session_id": session_id,
        "error": error,
        "error_type": error_type,
    }


def load_preview_plan(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """현재 세션과 계획 ID에 맞는 미리보기 계획을 불러옵니다.

    Args:
        state: 세션 ID와 계획 ID를 포함한 공유 상태입니다.

    Returns:
        저장된 ``PreviewPlan`` 또는 계획을 찾지 못했다는 오류 정보입니다.
    """
    if state.get("error"):
        return {}

    session_id = state.get("session_id") or "default-session"
    plan_id = state.get("plan_id") or ""

    preview_plan = preview_plan_store.get(session_id, plan_id)
    if not preview_plan:
        return {
            "error": f"Preview plan with id '{plan_id}' was not found in session '{session_id}'.",
            "error_type": "plan_not_found",
        }

    return {
        "preview_plan": preview_plan,
        "goal": preview_plan.goal,
    }


def verify_plan_not_expired(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """미리보기 계획의 승인 가능 시간이 지나지 않았는지 확인합니다.

    Args:
        state: 저장소에서 조회한 미리보기 계획을 포함한 공유 상태입니다.

    Returns:
        계획이 만료되었으면 오류 정보를, 유효하면 빈 변경값을 반환합니다.
    """
    if state.get("error") or not state.get("preview_plan"):
        return {}

    preview_plan = state["preview_plan"]
    if preview_plan_store.is_expired(preview_plan):
        return {
            "error": "The selected optimization plan has expired (5-minute validity exceeded).",
            "error_type": "plan_expired",
        }
    return {}


def verify_factory_revision(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """분석 시점과 적용 시점의 공장 revision 충돌을 검사합니다.

    Args:
        state: 미리보기 계획과 최신 공장 revision을 포함한 공유 상태입니다.

    Returns:
        revision이 다르면 충돌 오류를, 같으면 빈 변경값을 반환합니다.
    """
    if state.get("error") or not state.get("preview_plan"):
        return {}

    preview_plan = state["preview_plan"]
    current_revision = state.get("factoryRevision", 0)

    if preview_plan_store.check_revision_conflict(preview_plan, current_revision):
        return {
            "error": f"Factory revision conflict detected. Request revision: {current_revision}, Plan revision: {preview_plan.factoryRevision}.",
            "error_type": "revision_conflict",
        }
    return {}


def validate_approval(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """실행 명령을 만들기 전에 플레이어의 명시적 승인을 확인합니다.

    Args:
        state: 적용 요청의 승인 여부를 포함한 공유 상태입니다.

    Returns:
        승인이 없으면 ``approval_required`` 오류를 반환합니다.
    """
    if state.get("error"):
        return {}

    payload = state.get("payload", {})
    approval = payload.get("approval")

    if approval is not True:
        return {
            "error": "Optimization apply requires explicit approval (approval=true).",
            "error_type": "approval_required",
        }
    return {}


def validate_selected_changes(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """선택한 변경 ID가 계획에 존재하는지 검사하고 승인 목록을 만듭니다.

    Args:
        state: 미리보기 계획과 플레이어가 선택한 변경 ID를 포함한 상태입니다.

    Returns:
        검증된 ``approved_changes`` 또는 잘못된 선택에 대한 오류 정보입니다.
    """
    if state.get("error") or not state.get("preview_plan"):
        return {}

    preview_plan = state["preview_plan"]
    approved_change_ids = state.get("approved_change_ids")

    all_changes = {c.id: c for c in preview_plan.changes}
    approved_changes = []

    if approved_change_ids is None:
        approved_changes = list(preview_plan.changes)
    else:
        if not approved_change_ids:
            return {
                "error": "At least one change_id must be selected when approved_change_ids is provided.",
                "error_type": "no_changes_selected",
            }
        for cid in approved_change_ids:
            if cid not in all_changes:
                return {
                    "error": f"Change ID '{cid}' is not a valid component of this plan.",
                    "error_type": "invalid_change_id",
                }
            approved_changes.append(all_changes[cid])

    return {"approved_changes": approved_changes}


def build_unreal_commands(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """승인된 변경 제안을 구조화된 Unreal 명령으로 변환합니다.

    Args:
        state: 검증을 통과한 승인 변경 목록을 포함한 공유 상태입니다.

    Returns:
        Unreal에서 실행할 명령 payload 목록입니다.
    """
    if state.get("error") or not state.get("approved_changes"):
        return {}

    approved_changes = state["approved_changes"]
    commands = []

    for sug in approved_changes:
        cmd_payload = build_command_payload(sug)
        commands.append(cmd_payload)

    return {"commands": commands}


def validate_command_payloads(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """생성된 모든 명령이 허용된 명령 스키마와 일치하는지 검사합니다.

    Args:
        state: Unreal 명령 목록을 포함한 그래프 공유 상태입니다.

    Returns:
        잘못된 명령이 있으면 오류를, 모두 유효하면 빈 변경값을 반환합니다.
    """
    if state.get("error") or not state.get("commands"):
        return {}

    commands = state["commands"]
    for cmd in commands:
        if not validate_command_payload(cmd):
            return {
                "error": "Unreal execution command validation failed (forbidden command or parameter layout).",
                "error_type": "invalid_command_payload",
            }
    return {}


def _state_snapshot_by_change_id(raw_states: Any) -> dict[str, dict[str, Any]]:
    """Unreal의 변경 전후 snapshot을 변경 ID 기준 딕셔너리로 정규화합니다.

    Args:
        raw_states: 딕셔너리 또는 목록 형태로 전달된 변경 항목별 상태입니다.

    Returns:
        ``change_id``를 키로 사용하는 정규화된 snapshot 딕셔너리입니다.
    """
    if not raw_states:
        return {}

    if isinstance(raw_states, dict):
        normalized = {}
        for change_id, snapshot in raw_states.items():
            if isinstance(snapshot, dict):
                normalized[str(change_id)] = dict(snapshot)
        return normalized

    if isinstance(raw_states, list):
        normalized = {}
        for item in raw_states:
            if not isinstance(item, dict):
                continue
            change_id = item.get("change_id") or item.get("id")
            snapshot = (
                item.get("state") if isinstance(item.get("state"), dict) else item
            )
            if change_id and isinstance(snapshot, dict):
                snapshot_value = dict(snapshot)
                snapshot_value.pop("change_id", None)
                normalized[str(change_id)] = snapshot_value
        return normalized

    return {}


def _build_authoritative_state(
    snapshot_by_change_id: dict[str, dict[str, Any]],
    change_id: str,
    *,
    source: str,
    target: Any,
    planned_command: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    """Unreal이 명시적으로 보낸 snapshot으로 신뢰 가능한 기록 상태를 만듭니다.

    Args:
        snapshot_by_change_id: 변경 ID별로 정규화된 snapshot입니다.
        change_id: 상태를 찾을 변경 항목 식별자입니다.
        source: snapshot이 생성된 단계 또는 출처입니다.
        target: 변경 제안이 가리키는 공장 구성 요소입니다.
        planned_command: 적용 예정이거나 실행된 Unreal 명령입니다.

    Returns:
        실행 기록에 저장할 상태이며, snapshot이 없으면 ``None``입니다.
    """
    snapshot = snapshot_by_change_id.get(change_id)
    if not snapshot:
        return None

    state_value = dict(snapshot)
    state_value.setdefault("state_known", True)
    state_value.setdefault("source", source)
    if target:
        state_value.setdefault("target", target.model_dump())
    if planned_command is not None:
        state_value.setdefault("planned_command", planned_command)
    return state_value


def create_execution_record(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """승인된 변경별 실행 기록을 만들되 추정 상태를 실제 상태로 취급하지 않습니다.

    Args:
        state: 승인 변경, 명령, Unreal snapshot과 revision을 포함한 상태입니다.

    Returns:
        새로 저장했거나 이미 존재하는 실행 기록 목록입니다.
    """
    if state.get("error") or not state.get("approved_changes"):
        return {}

    plan_id = state["plan_id"] or "unknown-plan"
    approved_changes = state["approved_changes"]
    revision = state.get("factoryRevision", 0)
    commands = state.get("commands") or []
    factory_state = state.get("factory_state")
    before_snapshots = _state_snapshot_by_change_id(state.get("before_states"))
    after_snapshots = _state_snapshot_by_change_id(state.get("after_states"))

    records = []
    for index, sug in enumerate(approved_changes):
        if execution_record_store.has_record(plan_id, sug.id):
            return {
                "error": f"Change ID '{sug.id}' has already been processed for this plan.",
                "error_type": "duplicate_execution",
            }

        command_payload = commands[index] if index < len(commands) else {}
        before_val = _build_authoritative_state(
            before_snapshots,
            sug.id,
            source="unreal_pre_apply_snapshot",
            target=sug.target,
        )
        if before_val is None:
            before_val = {
                "state_known": False,
                "source": "unreal_runtime_required",
                "reason": "Apply request did not include an authoritative before state.",
                "target": sug.target.model_dump() if sug.target else None,
            }

        if before_val.get("state_known") is False and factory_state and sug.target:
            current_props = get_component_state(
                factory_state, sug.target.type, sug.target.id
            )
            if current_props:
                before_val.update(current_props)
                before_val["state_known"] = True
                before_val["source"] = "apply_factory_state_snapshot"

        after_val = _build_authoritative_state(
            after_snapshots,
            sug.id,
            source="unreal_post_apply_snapshot",
            target=sug.target,
            planned_command=command_payload,
        )
        if after_val is None:
            after_val = {
                "state_known": False,
                "source": "planned_command",
                "requires_unreal_confirmation": True,
                "planned_command": command_payload,
            }

        record = ExecutionRecord(
            plan_id=plan_id,
            change_id=sug.id,
            before=before_val,
            after=after_val,
            revision=revision,
        )
        execution_record_store.save(record)
        records.append(record)

    return {"execution_records": records}


def return_command_payload(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """검증된 실행 명령 또는 차단 오류를 최종 적용 응답으로 구성합니다.

    Args:
        state: 명령, 실행 기록과 오류 정보를 포함한 공유 상태입니다.

    Returns:
        Unreal에 전달할 ``execute_ready`` 응답 또는 안전한 오류 payload입니다.
    """
    if state.get("error"):
        error_type = state.get("error_type") or "error"
        error_payload = {
            "status": error_type,
            "factoryRevision": state.get("factoryRevision", 0),
            "goal": state.get("goal", "balance"),
            "summary": f"최적화 계획 실행 준비 실패: {state['error']}",
            "changes": [],
            "approved_changes": [],
            "expected_effect": {},
            "commands": [],
        }
        return {"previewPayload": error_payload}

    preview_plan = state["preview_plan"]
    approved_changes = state.get("approved_changes", [])
    commands = state.get("commands", [])

    preview_payload = {
        "status": "execute_ready",
        "plan_id": preview_plan.plan_id,
        "factoryRevision": state["factoryRevision"],
        "goal": preview_plan.goal,
        "summary": "최적화 계획 실행 준비가 완료되었습니다.",
        "changes": [c.model_dump() for c in preview_plan.changes],
        "approved_changes": [c.model_dump() for c in approved_changes],
        "expected_effect": preview_plan.expected_effect,
        "ui_hints": preview_plan.ui_hints.model_dump()
        if preview_plan.ui_hints
        else {"highlight_targets": []},
        "commands": commands,
    }

    return {"previewPayload": preview_payload}


# ==========================================
# 3. Undo (되돌리기) 흐름 노드들
# ==========================================


def load_execution_record(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """요청한 계획 ID에 속하는 실행 기록을 모두 불러옵니다.

    Args:
        state: 조회할 계획 ID를 포함한 공유 상태입니다.

    Returns:
        실행 기록 목록 또는 기록을 찾지 못했다는 오류 정보입니다.
    """
    payload = state.get("payload", {})
    plan_id = payload.get("plan_id")

    if not plan_id:
        return {
            "error": "Required parameter 'plan_id' is missing for undo operation.",
            "error_type": "record_not_found",
        }

    records = execution_record_store.get_records_by_plan(plan_id)
    if not records:
        return {
            "error": f"Execution records for plan_id '{plan_id}' were not found.",
            "error_type": "record_not_found",
        }

    revision = records[0].revision
    return {
        "plan_id": plan_id,
        "execution_records": records,
        "factoryRevision": revision,
    }


def validate_undo_request(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """되돌리기 충돌 검사에 필요한 최신 공장 상태가 있는지 확인합니다.

    Args:
        state: 되돌리기 요청과 공장 snapshot을 포함한 공유 상태입니다.

    Returns:
        상태가 없으면 검증 오류를, 있으면 빈 변경값을 반환합니다.
    """
    if state.get("error"):
        return {}

    payload = state.get("payload", {})
    factory_state = payload.get("factory_state")

    if not factory_state and payload and "machines" in payload:
        factory_state = payload

    if not factory_state:
        return {
            "error": "Factory state is missing in payload for undo operation.",
            "error_type": "invalid_factory_state",
        }

    session_id = (
        state.get("session_id")
        or payload.get("session_id")
        or payload.get("session-id")
        or "default-session"
    )

    return {"factory_state": factory_state, "session_id": session_id}


def compare_current_state_with_recorded_after_state(
    state: ProcessOptimizerGraphState,
) -> dict[str, Any]:
    """각 실행 기록의 적용 후 상태를 현재 공장 상태와 비교합니다.

    Args:
        state: 실행 기록과 최신 공장 snapshot을 포함한 공유 상태입니다.

    Returns:
        플레이어 수정 등으로 값이 달라진 변경 ID 목록입니다.
    """
    if state.get("error"):
        return {}

    records = state.get("execution_records", [])
    factory_state = state["factory_state"]

    conflicts = []
    for rec in records:
        if check_undo_conflict(rec, factory_state):
            conflicts.append(rec.change_id)

    return {"conflicts": conflicts}


def conflict_check(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """하나라도 충돌한 변경이 있으면 자동 되돌리기를 중단합니다.

    Args:
        state: 충돌한 변경 ID 목록을 포함한 공유 상태입니다.

    Returns:
        충돌 오류 정보 또는 계속 진행하기 위한 빈 변경값입니다.
    """
    if state.get("error"):
        return {}

    conflicts = state.get("conflicts", [])
    if conflicts:
        return {
            "error": f"Undo conflict detected on change IDs: {conflicts}. Current factory state has been modified.",
            "error_type": "undo_conflict",
        }
    return {}


def build_inverse_commands(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """실행 기록의 적용 전 값을 사용해 역방향 명령을 만듭니다.

    Args:
        state: 충돌 검사를 통과한 실행 기록 목록을 포함한 상태입니다.

    Returns:
        Unreal에서 실행할 되돌리기 명령 목록 또는 생성 실패 오류입니다.
    """
    if state.get("error"):
        return {}

    records = state.get("execution_records", [])
    commands = []

    for rec in records:
        inv_cmd = build_inverse_command(rec)
        if inv_cmd:
            commands.append(inv_cmd)
        else:
            return {
                "error": f"Failed to build inverse command for change_id '{rec.change_id}'. Before state may be unknown.",
                "error_type": "undo_conflict",
            }

    return {"commands": commands}


def return_undo_command_payload(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """역방향 명령 또는 차단 사유를 최종 되돌리기 응답으로 구성합니다.

    Args:
        state: 역방향 명령과 오류 정보를 포함한 공유 상태입니다.

    Returns:
        ``undo_ready`` 응답 또는 충돌·검증 오류 payload입니다.
    """
    if state.get("error"):
        error_type = state.get("error_type") or "error"
        error_payload = {
            "status": error_type,
            "factoryRevision": state.get("factoryRevision", 0),
            "goal": state.get("goal", "balance"),
            "summary": f"되돌리기 계획 준비 실패: {state['error']}",
            "changes": [],
            "approved_changes": [],
            "expected_effect": {},
            "commands": [],
        }
        return {"previewPayload": error_payload}

    commands = state.get("commands", [])
    preview_payload = {
        "status": "undo_ready",
        "plan_id": state.get("plan_id"),
        "factoryRevision": state.get("factoryRevision", 0),
        "goal": state.get("goal", "balance"),
        "summary": "되돌리기 계획 준비가 완료되었습니다.",
        "changes": [],
        "approved_changes": [],
        "expected_effect": {"estimated": False},
        "ui_hints": {"highlight_targets": []},
        "commands": commands,
    }
    return {"previewPayload": preview_payload}


# ==========================================
# 4. Measure (효과 측정) 흐름 노드들
# ==========================================


def validate_measurement_window(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """성과 측정에 필요한 관찰 시간과 생산 주기 조건을 검사합니다.

    Args:
        state: 계획 ID, 현재 시각, 생산 주기를 포함한 측정 요청 상태입니다.

    Returns:
        측정 가능 상태 또는 아직 관찰이 부족하다는 오류 정보입니다.
    """
    payload = state.get("payload", {})
    plan_id = payload.get("plan_id")

    if not plan_id:
        return {
            "error": "Required parameter 'plan_id' is missing for measurement.",
            "error_type": "record_not_found",
        }

    cycles = payload.get("production_cycles", 0)
    if not check_production_cycles(cycles):
        return {
            "error": f"Insufficient production cycles: {cycles} (minimum 3 required).",
            "error_type": "measurement_not_ready",
        }

    records = execution_record_store.get_records_by_plan(plan_id)
    if not records:
        return {
            "error": f"Execution records for plan_id '{plan_id}' were not found.",
            "error_type": "record_not_found",
        }

    current_time_str = payload.get("current_time")
    current_time = None
    if current_time_str:
        try:
            current_time = datetime.fromisoformat(
                current_time_str.replace("Z", "+00:00")
            )
        except Exception:
            pass

    if not check_observation_window(records, current_time):
        return {
            "error": "Observation duration is insufficient (minimum 30 seconds required).",
            "error_type": "measurement_not_ready",
        }

    return {"plan_id": plan_id, "production_cycles": cycles}


def calculate_before_after_metrics(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """실행 기록을 이용해 적용 전후 공장 분석 지표를 계산합니다.

    Args:
        state: 최신 공장 상태와 실행 기록을 포함한 공유 상태입니다.

    Returns:
        복원한 적용 전 지표와 현재 적용 후 지표입니다.
    """
    if state.get("error"):
        return {}

    records = state.get("execution_records", [])
    factory_state = state.get("payload", {}).get("factory_state")
    if not factory_state:
        factory_state = state.get("payload", {})  # fallback

    if not factory_state or "machines" not in factory_state:
        return {
            "error": "Factory state is missing in payload for effect measurement.",
            "error_type": "invalid_factory_state",
        }

    for record in records:
        if (
            isinstance(record.before, dict)
            and record.before.get("state_known") is False
        ):
            return {
                "error": (
                    "Effect measurement requires authoritative before state. "
                    f"Change '{record.change_id}' only has a placeholder before state."
                ),
                "error_type": "measurement_not_ready",
            }

    before_state_dict = recreate_before_state(factory_state, records)

    analyzer = FactoryStateAnalyzerTool()
    try:
        before_metrics = analyzer.analyze(
            factory_state=before_state_dict,
            factory_revision=state.get("factoryRevision", 0),
            goal=state.get("goal", "balance"),
        )
        current_metrics = analyzer.analyze(
            factory_state=factory_state,
            factory_revision=state.get("factoryRevision", 0),
            goal=state.get("goal", "balance"),
        )
        return {"before_metrics": before_metrics, "metrics": current_metrics}
    except Exception as e:
        return {
            "error": f"Failed to calculate before/after metrics: {str(e)}",
            "error_type": "measurement_error",
        }


def compare_expected_and_actual_effects(
    state: ProcessOptimizerGraphState,
) -> dict[str, Any]:
    """계획의 예상 효과와 실제로 측정한 공장 변화를 비교합니다.

    Args:
        state: 미리보기 계획과 적용 전후 분석 지표를 포함한 상태입니다.

    Returns:
        실제 개선 수치와 임시 판정이 담긴 측정 결과 데이터입니다.
    """
    if state.get("error"):
        return {}

    plan_id = state.get("plan_id")
    session_id = state.get("session_id") or "default-session"

    preview_plan = preview_plan_store.get(session_id, plan_id)
    expected_effect = {
        "resolved_input_shortages_count": 0,
        "resolved_output_blocks_count": 0,
        "resolved_conveyor_congestions_count": 0,
    }

    if preview_plan and preview_plan.expected_effect:
        expected_effect.update(preview_plan.expected_effect)

    before_metrics = state.get("before_metrics")
    after_metrics = state.get("metrics")

    eval_result = evaluate_effects(expected_effect, before_metrics, after_metrics)
    eval_result["expected_effect"] = expected_effect

    return {"measurement_result_data": eval_result}


def classify_effect_result(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """측정 결과를 성공, 목표 미달, 악화 중 하나로 확정합니다.

    Args:
        state: 예상치와 실측치 비교 결과를 포함한 공유 상태입니다.

    Returns:
        다음 행동까지 포함한 최종 성과 측정 리포트입니다.
    """
    if state.get("error"):
        return {}

    eval_data = state.get("measurement_result_data", {})
    cycles = state.get("production_cycles", 0)

    records = state.get("execution_records", [])
    current_time_str = state.get("payload", {}).get("current_time")
    current_time = None
    if current_time_str:
        try:
            current_time = datetime.fromisoformat(
                current_time_str.replace("Z", "+00:00")
            )
        except Exception:
            pass

    if current_time is None:
        current_time = datetime.now(UTC)

    if current_time.tzinfo is None:
        current_time = current_time.replace(tzinfo=UTC)

    duration = 0.0
    if records:
        rec_time = records[0].created_at
        if rec_time.tzinfo is None:
            rec_time = rec_time.replace(tzinfo=UTC)
        duration = (current_time - rec_time).total_seconds()

    report = EffectMeasurementReport(
        status=eval_data["status"],
        next_action=eval_data["next_action"],
        expected_effect=eval_data["expected_effect"],
        actual_effect=eval_data["actual_effect"],
        observation_duration_seconds=duration,
        production_cycles=cycles,
    )

    return {
        "measurement_result": report,
        "commands": [],  # 자동 복구 복제 명령 미생성 안전장치
    }


def return_measurement_summary(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """성과 측정 결과 또는 측정 오류를 최종 응답으로 구성합니다.

    Args:
        state: 측정 리포트와 오류 정보를 포함한 그래프 공유 상태입니다.

    Returns:
        Unreal에 전달할 측정 결과 ``previewPayload``입니다.
    """
    if state.get("error"):
        error_type = state.get("error_type") or "error"
        error_payload = {
            "status": error_type,
            "factoryRevision": state.get("factoryRevision", 0),
            "goal": state.get("goal", "balance"),
            "summary": f"성능 효과 측정 실패: {state['error']}",
            "changes": [],
            "approved_changes": [],
            "expected_effect": {},
            "commands": [],
        }
        return {"previewPayload": error_payload}

    report = state.get("measurement_result")
    report_dict = report.model_dump() if report else {}

    status = "measurement_ready"
    summary_text = f"최적화 효과 측정 결과, 공장 가동 상태가 '{report.status}' 등급으로 분석되었습니다."
    if report.status == "degraded":
        summary_text += " 공장 성능이 이전보다 악화되었습니다. 최신 공장 상태에 대한 재분석(reanalyze)을 권장합니다."

    preview_payload = {
        "status": status,
        "plan_id": state.get("plan_id"),
        "factoryRevision": state.get("factoryRevision", 0),
        "goal": state.get("goal", "balance"),
        "summary": summary_text,
        "changes": [],
        "approved_changes": [],
        "expected_effect": {},
        "commands": [],
        "measurement_result": report_dict,
    }
    return {"previewPayload": preview_payload}
