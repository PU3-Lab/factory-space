"""LangGraph nodes for the Process Optimizer agent.

Each node receives the shared ProcessOptimizerGraphState and returns only the
fields that should be merged into the graph state.
"""

import uuid
from datetime import datetime, timedelta, timezone
from typing import Any
from agents.process_optimizer.graph_state import ProcessOptimizerGraphState
from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
from agents.process_optimizer.suggestion import (
    OptimizationSuggestionTool,
    SuggestionValidationTool,
)
from agents.process_optimizer.schemas import (
    EffectMeasurementReport,
    FactoryState,
    PreviewPlan,
    UiHints,
)
from agents.process_optimizer.preview_store import preview_plan_store
from agents.process_optimizer.commands import build_command_payload, validate_command_payload
from agents.process_optimizer.execution_record import ExecutionRecord, execution_record_store
from agents.process_optimizer.undo import check_undo_conflict, build_inverse_command, get_component_state
from agents.process_optimizer.effect_measurement import (
    check_observation_window,
    check_production_cycles,
    recreate_before_state,
    evaluate_effects,
)


# ==========================================
# 0. Common routing nodes
# ==========================================

def route_operation(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Read the requested operation and route the graph to the matching branch."""
    payload = state.get("payload", {})
    operation = payload.get("operation") or "analyze"
    return {"operation": operation}


# ==========================================
# 1. Analyze flow nodes
# ==========================================

def validate_factory_state(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Validate the incoming factory state and initialize common parameters."""
    payload = state.get("payload", {})
    context = state.get("context", {}) or {}
    operation = payload.get("operation") or "analyze"
    goal = payload.get("goal") or "balance"
    
    session_id = state.get("session_id") or payload.get("session_id") or payload.get("session-id") or "default-session"
    
    factory_state = payload.get("factory_state")
    revision = payload.get("factoryRevision")
    if revision is None:
        revision = context.get("factoryRevision")
    
    if not factory_state and payload and "machines" in payload:
        factory_state = payload
        
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
    """Calculate deterministic factory metrics from the current state."""
    if state.get("error"):
        return {}
        
    analyzer = FactoryStateAnalyzerTool()
    try:
        report = analyzer.analyze(
            factory_state=state["factory_state"],
            factory_revision=state["factoryRevision"],
            goal=state["goal"],
        )
        return {"metrics": report}
    except Exception as e:
        return {"error": f"Failed to calculate metrics: {str(e)}"}

def detect_bottlenecks(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Extract bottleneck categories from the calculated metrics."""
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
    """Build preview optimization suggestions from the metric report."""
    if state.get("error") or not state.get("metrics"):
        return {}
        
    metrics = state["metrics"]
    suggestion_tool = OptimizationSuggestionTool()
    suggestions, ui_hints = suggestion_tool.generate_suggestions(metrics)
            
    return {
        "suggestions": suggestions,
        "ui_hints": ui_hints,
    }

def validate_preview_candidates(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Validate preview suggestions against safety and business rules."""
    if state.get("error") or not state.get("suggestions"):
        return {}
        
    validator = SuggestionValidationTool()
    is_valid = validator.validate_suggestions(state["suggestions"])
    
    if not is_valid:
        return {"error": "Optimization suggestions failed safety and business rule validation."}
        
    return {}

def create_preview_plan(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Create a temporary PreviewPlan from validated suggestions."""
    if state.get("error") or not state.get("suggestions"):
        return {}
        
    suggestions = state["suggestions"]
    ui_hints = state.get("ui_hints") or UiHints()
    
    plan_id = f"plan-{uuid.uuid4().hex[:8]}"
    created_at = datetime.now(timezone.utc)
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
    """Store the generated PreviewPlan in the in-memory preview store."""
    if state.get("error") or not state.get("preview_plan"):
        return {}
        
    preview_plan = state["preview_plan"]
    preview_plan_store.save(preview_plan)
    return {}

def return_preview_plan(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Build the final preview response payload."""
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
        "ui_hints": preview_plan.ui_hints.model_dump() if preview_plan.ui_hints else {"highlight_targets": []},
        "expires_at": preview_plan.expires_at.isoformat(),
    }
    
    return {"previewPayload": preview_payload}


# ==========================================
# 2. Apply flow nodes
# ==========================================

def validate_apply_request(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Validate the apply request payload and required parameters."""
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
    
    session_id = state.get("session_id") or payload.get("session_id") or payload.get("session-id") or "default-session"
    
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
    """Load the stored preview plan for the current session and plan id."""
    if state.get("error"):
        return {}
        
    session_id = state.get("session_id") or "default-session"
    plan_id = state.get("plan_id") or ""
    
    preview_plan = preview_plan_store.get(session_id, plan_id)
    if not preview_plan:
        return {
            "error": f"Preview plan with id '{plan_id}' was not found in session '{session_id}'.",
            "error_type": "plan_not_found"
        }
        
    return {
        "preview_plan": preview_plan,
        "goal": preview_plan.goal,
    }

def verify_plan_not_expired(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Reject a preview plan after its five-minute validity window expires."""
    if state.get("error") or not state.get("preview_plan"):
        return {}
        
    preview_plan = state["preview_plan"]
    if preview_plan_store.is_expired(preview_plan):
        return {
            "error": "The selected optimization plan has expired (5-minute validity exceeded).",
            "error_type": "plan_expired"
        }
    return {}

def verify_factory_revision(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Detect stale plans by comparing request and preview factory revisions."""
    if state.get("error") or not state.get("preview_plan"):
        return {}
        
    preview_plan = state["preview_plan"]
    current_revision = state.get("factoryRevision", 0)
    
    if preview_plan_store.check_revision_conflict(preview_plan, current_revision):
        return {
            "error": f"Factory revision conflict detected. Request revision: {current_revision}, Plan revision: {preview_plan.factoryRevision}.",
            "error_type": "revision_conflict"
        }
    return {}

def validate_approval(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Require explicit player approval before execution commands are prepared."""
    if state.get("error"):
        return {}
        
    payload = state.get("payload", {})
    approval = payload.get("approval")
    
    if approval is not True:
        return {
            "error": "Optimization apply requires explicit approval (approval=true).",
            "error_type": "approval_required"
        }
    return {}

def validate_selected_changes(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Validate selected change IDs and build the approved change list."""
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
                "error_type": "no_changes_selected"
            }
        for cid in approved_change_ids:
            if cid not in all_changes:
                return {
                    "error": f"Change ID '{cid}' is not a valid component of this plan.",
                    "error_type": "invalid_change_id"
                }
            approved_changes.append(all_changes[cid])
            
    return {"approved_changes": approved_changes}

def build_unreal_commands(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Convert approved changes into structured Unreal command payloads."""
    if state.get("error") or not state.get("approved_changes"):
        return {}
        
    approved_changes = state["approved_changes"]
    commands = []
    
    for sug in approved_changes:
        cmd_payload = build_command_payload(sug)
        commands.append(cmd_payload)
        
    return {"commands": commands}

def validate_command_payloads(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Validate that each command matches the allowed command schema."""
    if state.get("error") or not state.get("commands"):
        return {}
        
    commands = state["commands"]
    for cmd in commands:
        if not validate_command_payload(cmd):
            return {
                "error": "Unreal execution command validation failed (forbidden command or parameter layout).",
                "error_type": "invalid_command_payload"
            }
    return {}

def _state_snapshot_by_change_id(raw_states: Any) -> dict[str, dict[str, Any]]:
    """Normalize Unreal-provided before/after snapshots by change_id."""
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
            snapshot = item.get("state") if isinstance(item.get("state"), dict) else item
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
    """Build an execution-record state from an explicit Unreal snapshot."""
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
    """Create execution records without pretending guessed before/after state is authoritative."""
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
                "error_type": "duplicate_execution"
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
            current_props = get_component_state(factory_state, sug.target.type, sug.target.id)
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
    """Build the final execute_ready response or a guarded error payload."""
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
        "ui_hints": preview_plan.ui_hints.model_dump() if preview_plan.ui_hints else {"highlight_targets": []},
        "commands": commands,
    }
    
    return {"previewPayload": preview_payload}


# ==========================================
# 3. Undo (되돌리기) 흐름 노드들
# ==========================================

def load_execution_record(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Load all execution records for the requested plan_id."""
    payload = state.get("payload", {})
    plan_id = payload.get("plan_id")

    if not plan_id:
        return {
            "error": "Required parameter 'plan_id' is missing for undo operation.",
            "error_type": "record_not_found"
        }

    records = execution_record_store.get_records_by_plan(plan_id)
    if not records:
        return {
            "error": f"Execution records for plan_id '{plan_id}' were not found.",
            "error_type": "record_not_found"
        }

    revision = records[0].revision
    return {
        "plan_id": plan_id,
        "execution_records": records,
        "factoryRevision": revision
    }

def validate_undo_request(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Validate that the current factory state was provided for undo."""
    if state.get("error"):
        return {}

    payload = state.get("payload", {})
    factory_state = payload.get("factory_state")

    if not factory_state and payload and "machines" in payload:
        factory_state = payload

    if not factory_state:
        return {
            "error": "Factory state is missing in payload for undo operation.",
            "error_type": "invalid_factory_state"
        }

    session_id = state.get("session_id") or payload.get("session_id") or payload.get("session-id") or "default-session"

    return {
        "factory_state": factory_state,
        "session_id": session_id
    }

def compare_current_state_with_recorded_after_state(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Compare recorded after-state values with the current factory state."""
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
    """Stop undo when any recorded change conflicts with the current factory state."""
    if state.get("error"):
        return {}

    conflicts = state.get("conflicts", [])
    if conflicts:
        return {
            "error": f"Undo conflict detected on change IDs: {conflicts}. Current factory state has been modified.",
            "error_type": "undo_conflict"
        }
    return {}

def build_inverse_commands(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Build inverse commands from recorded before-state values."""
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
                "error_type": "undo_conflict"
            }

    return {"commands": commands}

def return_undo_command_payload(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Build the final undo_ready response or guarded error payload."""
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
    """Validate the minimum observation time and production cycle requirements."""
    payload = state.get("payload", {})
    plan_id = payload.get("plan_id")

    if not plan_id:
        return {
            "error": "Required parameter 'plan_id' is missing for measurement.",
            "error_type": "record_not_found"
        }

    cycles = payload.get("production_cycles", 0)
    if not check_production_cycles(cycles):
        return {
            "error": f"Insufficient production cycles: {cycles} (minimum 3 required).",
            "error_type": "measurement_not_ready"
        }

    records = execution_record_store.get_records_by_plan(plan_id)
    if not records:
        return {
            "error": f"Execution records for plan_id '{plan_id}' were not found.",
            "error_type": "record_not_found"
        }

    current_time_str = payload.get("current_time")
    current_time = None
    if current_time_str:
        try:
            current_time = datetime.fromisoformat(current_time_str.replace("Z", "+00:00"))
        except Exception:
            pass

    if not check_observation_window(records, current_time):
        return {
            "error": "Observation duration is insufficient (minimum 30 seconds required).",
            "error_type": "measurement_not_ready"
        }

    return {
        "plan_id": plan_id,
        "production_cycles": cycles
    }

def calculate_before_after_metrics(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Calculate before and after metrics for effect measurement."""
    if state.get("error"):
        return {}

    records = state.get("execution_records", [])
    factory_state = state.get("payload", {}).get("factory_state")
    if not factory_state:
        factory_state = state.get("payload", {}) # fallback

    if not factory_state or "machines" not in factory_state:
        return {
            "error": "Factory state is missing in payload for effect measurement.",
            "error_type": "invalid_factory_state"
        }

    for record in records:
        if isinstance(record.before, dict) and record.before.get("state_known") is False:
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
            goal=state.get("goal", "balance")
        )
        current_metrics = analyzer.analyze(
            factory_state=factory_state,
            factory_revision=state.get("factoryRevision", 0),
            goal=state.get("goal", "balance")
        )
        return {
            "before_metrics": before_metrics,
            "metrics": current_metrics
        }
    except Exception as e:
        return {
            "error": f"Failed to calculate before/after metrics: {str(e)}",
            "error_type": "measurement_error"
        }

def compare_expected_and_actual_effects(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Compare expected effects with measured factory effects."""
    if state.get("error"):
        return {}

    plan_id = state.get("plan_id")
    session_id = state.get("session_id") or "default-session"

    preview_plan = preview_plan_store.get(session_id, plan_id)
    expected_effect = {
        "resolved_input_shortages_count": 0,
        "resolved_output_blocks_count": 0,
        "resolved_conveyor_congestions_count": 0
    }

    if preview_plan and preview_plan.expected_effect:
        expected_effect.update(preview_plan.expected_effect)

    before_metrics = state.get("before_metrics")
    after_metrics = state.get("metrics")

    eval_result = evaluate_effects(expected_effect, before_metrics, after_metrics)
    eval_result["expected_effect"] = expected_effect

    return {"measurement_result_data": eval_result}

def classify_effect_result(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Classify measured effects as success, failed, or degraded."""
    if state.get("error"):
        return {}

    eval_data = state.get("measurement_result_data", {})
    cycles = state.get("production_cycles", 0)

    records = state.get("execution_records", [])
    current_time_str = state.get("payload", {}).get("current_time")
    current_time = None
    if current_time_str:
        try:
            current_time = datetime.fromisoformat(current_time_str.replace("Z", "+00:00"))
        except Exception:
            pass

    if current_time is None:
        current_time = datetime.now(timezone.utc)

    if current_time.tzinfo is None:
        current_time = current_time.replace(tzinfo=timezone.utc)

    duration = 0.0
    if records:
        rec_time = records[0].created_at
        if rec_time.tzinfo is None:
            rec_time = rec_time.replace(tzinfo=timezone.utc)
        duration = (current_time - rec_time).total_seconds()

    report = EffectMeasurementReport(
        status=eval_data["status"],
        next_action=eval_data["next_action"],
        expected_effect=eval_data["expected_effect"],
        actual_effect=eval_data["actual_effect"],
        observation_duration_seconds=duration,
        production_cycles=cycles
    )

    return {
        "measurement_result": report,
        "commands": []  # 자동 복구 복제 명령 미생성 안전장치
    }

def return_measurement_summary(state: ProcessOptimizerGraphState) -> dict[str, Any]:
    """Build the final effect measurement response payload."""
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
        "measurement_result": report_dict
    }
    return {"previewPayload": preview_payload}
