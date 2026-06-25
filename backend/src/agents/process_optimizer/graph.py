"""LangGraph construction for Process Optimizer.

초보자 설명:
이 파일은 여러 노드(Node)들을 연결하여 하나의 실행 가능한 워크플로우(Graph)로 조립(compile)합니다.
노드 간의 실행 순서와 흐름을 Edge를 통해 제어합니다.
"""

from typing import Any
from langgraph.graph import StateGraph, START, END
from agents.process_optimizer.graph_state import ProcessOptimizerGraphState
from agents.process_optimizer.nodes import (
    route_operation,
    validate_factory_state,
    calculate_metrics,
    detect_bottlenecks,
    build_optimization_candidates,
    validate_preview_candidates,
    create_preview_plan,
    save_preview_plan,
    return_preview_plan,
    validate_apply_request,
    load_preview_plan,
    verify_plan_not_expired,
    verify_factory_revision,
    validate_approval,
    validate_selected_changes,
    build_unreal_commands,
    validate_command_payloads,
    create_execution_record,
    return_command_payload,
    load_execution_record,
    validate_undo_request,
    compare_current_state_with_recorded_after_state,
    conflict_check,
    build_inverse_commands,
    return_undo_command_payload,
    validate_measurement_window,
    calculate_before_after_metrics,
    compare_expected_and_actual_effects,
    classify_effect_result,
    return_measurement_summary,
)

def get_next_step_after_load_record(state: ProcessOptimizerGraphState) -> str:
    """load_execution_record 노드 완료 후 operation에 따라 Undo 흐름 혹은 Measure 흐름으로 라우팅합니다."""
    if state.get("operation") == "measure":
        return "calculate_before_after_metrics"
    return "validate_undo_request"

def get_next_step(state: ProcessOptimizerGraphState) -> str:
    """operation 값에 따라 'analyze', 'apply', 'undo', 또는 'measure' 실행 노드로 분기(Routing) 경로를 지정합니다."""
    if state.get("operation") == "apply":
        return "validate_apply_request"
    elif state.get("operation") == "undo":
        return "load_execution_record"
    elif state.get("operation") == "measure":
        return "validate_measurement_window"
    return "validate_factory_state"

def compile_process_optimizer_graph():
    """Process Optimizer 전용 LangGraph를 빌드하고 컴파일하여 반환합니다.
    
    설명:
    StateGraph에 사용할 상태(ProcessOptimizerGraphState)를 지정하고,
    operation에 따른 조건부 라우팅을 설정한 후 각각의 흐름을 엣지로 연결합니다.
    """
    # 1. 상태 클래스를 기반으로 그래프 초기화
    workflow = StateGraph(ProcessOptimizerGraphState)
    
    # 2. 모든 분석 및 적용 노드 등록
    workflow.add_node("route_operation", route_operation)
    
    # Analyze 흐름 노드들
    workflow.add_node("validate_factory_state", validate_factory_state)
    workflow.add_node("calculate_metrics", calculate_metrics)
    workflow.add_node("detect_bottlenecks", detect_bottlenecks)
    workflow.add_node("build_optimization_candidates", build_optimization_candidates)
    workflow.add_node("validate_preview_candidates", validate_preview_candidates)
    workflow.add_node("create_preview_plan", create_preview_plan)
    workflow.add_node("save_preview_plan", save_preview_plan)
    workflow.add_node("return_preview_plan", return_preview_plan)
    
    # Apply 흐름 노드들
    workflow.add_node("validate_apply_request", validate_apply_request)
    workflow.add_node("load_preview_plan", load_preview_plan)
    workflow.add_node("verify_plan_not_expired", verify_plan_not_expired)
    workflow.add_node("verify_factory_revision", verify_factory_revision)
    workflow.add_node("validate_approval", validate_approval)
    workflow.add_node("validate_selected_changes", validate_selected_changes)
    workflow.add_node("build_unreal_commands", build_unreal_commands)
    workflow.add_node("validate_command_payloads", validate_command_payloads)
    workflow.add_node("create_execution_record", create_execution_record)
    workflow.add_node("return_command_payload", return_command_payload)

    # Undo 흐름 노드 등록
    workflow.add_node("load_execution_record", load_execution_record)
    workflow.add_node("validate_undo_request", validate_undo_request)
    workflow.add_node("compare_current_state_with_recorded_after_state", compare_current_state_with_recorded_after_state)
    workflow.add_node("conflict_check", conflict_check)
    workflow.add_node("build_inverse_commands", build_inverse_commands)
    workflow.add_node("return_undo_command_payload", return_undo_command_payload)

    # Measure 흐름 노드 등록
    workflow.add_node("validate_measurement_window", validate_measurement_window)
    workflow.add_node("calculate_before_after_metrics", calculate_before_after_metrics)
    workflow.add_node("compare_expected_and_actual_effects", compare_expected_and_actual_effects)
    workflow.add_node("classify_effect_result", classify_effect_result)
    workflow.add_node("return_measurement_summary", return_measurement_summary)
    
    # 3. 엣지 연결
    workflow.add_edge(START, "route_operation")
    
    # route_operation 이후 조건부 라우팅 분기
    workflow.add_conditional_edges(
        "route_operation",
        get_next_step,
        {
            "validate_factory_state": "validate_factory_state",
            "validate_apply_request": "validate_apply_request",
            "load_execution_record": "load_execution_record",
            "validate_measurement_window": "validate_measurement_window",
        }
    )
    
    # 3.1 Analyze 흐름 엣지 연결
    workflow.add_edge("validate_factory_state", "calculate_metrics")
    workflow.add_edge("calculate_metrics", "detect_bottlenecks")
    workflow.add_edge("detect_bottlenecks", "build_optimization_candidates")
    workflow.add_edge("build_optimization_candidates", "validate_preview_candidates")
    workflow.add_edge("validate_preview_candidates", "create_preview_plan")
    workflow.add_edge("create_preview_plan", "save_preview_plan")
    workflow.add_edge("save_preview_plan", "return_preview_plan")
    workflow.add_edge("return_preview_plan", END)
    
    # 3.2 Apply 흐름 엣지 연결
    workflow.add_edge("validate_apply_request", "load_preview_plan")
    workflow.add_edge("load_preview_plan", "verify_plan_not_expired")
    workflow.add_edge("verify_plan_not_expired", "verify_factory_revision")
    workflow.add_edge("verify_factory_revision", "validate_approval")
    workflow.add_edge("validate_approval", "validate_selected_changes")
    workflow.add_edge("validate_selected_changes", "build_unreal_commands")
    workflow.add_edge("build_unreal_commands", "validate_command_payloads")
    workflow.add_edge("validate_command_payloads", "create_execution_record")
    workflow.add_edge("create_execution_record", "return_command_payload")
    workflow.add_edge("return_command_payload", END)

    # 3.3 load_execution_record 이후 조건부 라우팅 분기
    workflow.add_conditional_edges(
        "load_execution_record",
        get_next_step_after_load_record,
        {
            "validate_undo_request": "validate_undo_request",
            "calculate_before_after_metrics": "calculate_before_after_metrics"
        }
    )

    # 3.4 Undo 흐름 엣지 연결
    workflow.add_edge("validate_undo_request", "compare_current_state_with_recorded_after_state")
    workflow.add_edge("compare_current_state_with_recorded_after_state", "conflict_check")
    workflow.add_edge("conflict_check", "build_inverse_commands")
    workflow.add_edge("build_inverse_commands", "return_undo_command_payload")
    workflow.add_edge("return_undo_command_payload", END)

    # 3.5 Measure 흐름 엣지 연결
    workflow.add_edge("validate_measurement_window", "load_execution_record")
    workflow.add_edge("calculate_before_after_metrics", "compare_expected_and_actual_effects")
    workflow.add_edge("compare_expected_and_actual_effects", "classify_effect_result")
    workflow.add_edge("classify_effect_result", "return_measurement_summary")
    workflow.add_edge("return_measurement_summary", END)
    
    # 4. 그래프 컴파일
    return workflow.compile()
