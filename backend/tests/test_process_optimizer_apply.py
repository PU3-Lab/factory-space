"""Unit tests for Process Optimizer LangGraph v2 Apply Validation Flow."""

from datetime import datetime, timedelta, timezone
import pytest
from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.schemas import PreviewPlan, UiHints, OptimizationSuggestion, TargetDescriptor
from agents.process_optimizer.preview_store import preview_plan_store
from agents.process_optimizer.execution_record import execution_record_store
from agents.process_optimizer.graph import compile_process_optimizer_graph

@pytest.fixture(autouse=True)
def clean_store():
    """각 테스트 격리성을 위해 저장소들을 비워줍니다."""
    preview_plan_store.clear()
    execution_record_store.clear()

def create_mock_plan(plan_id: str, session_id: str, revision: int = 10, expired: bool = False) -> PreviewPlan:
    """테스트용 Mock 프리뷰 계획을 생성합니다."""
    created_at = datetime.now(timezone.utc)
    if expired:
        created_at -= timedelta(minutes=10)
        expires_at = created_at + timedelta(minutes=5)  # 이미 5분 전에 만료됨
    else:
        expires_at = created_at + timedelta(minutes=5)
        
    changes = [
        OptimizationSuggestion(
            id="change-01",
            target=TargetDescriptor(type="machine", id="smelter_01"),
            problem="Input ore is low.",
            recommended_action="Check belts.",
            expected_effect="Resume production."
        ),
        OptimizationSuggestion(
            id="change-02",
            target=TargetDescriptor(type="machine", id="constructor_01"),
            problem="Output is blocked.",
            recommended_action="Clear chest.",
            expected_effect="Clear blockage."
        )
    ]
    
    return PreviewPlan(
        plan_id=plan_id,
        session_id=session_id,
        factoryRevision=revision,
        goal="balance",
        changes=changes,
        expected_effect={"resolved_count": 2},
        ui_hints=UiHints(highlight_targets=["smelter_01", "constructor_01"]),
        created_at=created_at,
        expires_at=expires_at
    )

def test_apply_without_approval_returns_error():
    """승인(approval) 값이 누락되거나 False인 경우 approval_required 에러가 리턴되는지 검증합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-apply-no-approval"
    session_id = "session-1"
    
    # 1. Mock 플랜 저장
    plan = create_mock_plan(plan_id, session_id)
    preview_plan_store.save(plan)
    
    # 2. approval 누락 케이스
    initial_state_missing = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            # approval 이 누락됨
        }
    }
    
    result = graph.invoke(initial_state_missing)
    assert result.get("error") is not None
    assert result.get("error_type") == "approval_required"
    
    preview = result.get("previewPayload")
    assert preview["status"] == "approval_required"
    assert "requires explicit approval" in preview["summary"]

    # 3. approval이 False인 케이스
    initial_state_false = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": False
        }
    }
    
    result_false = graph.invoke(initial_state_false)
    assert result_false.get("error_type") == "approval_required"

def test_apply_unknown_plan_returns_error():
    """저장소에 존재하지 않는 plan_id로 apply 시도 시 plan_not_found 에러가 리턴되는지 검증합니다."""
    graph = compile_process_optimizer_graph()
    
    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": "non-existent-plan-id",
            "session_id": "session-1",
            "factoryRevision": 10,
            "approval": True
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error_type") == "plan_not_found"
    assert result["previewPayload"]["status"] == "plan_not_found"

def test_apply_expired_plan_returns_error():
    """유효시간이 초과되어 만료된 플랜으로 apply 시도 시 plan_expired 에러가 리턴되는지 검증합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-expired"
    session_id = "session-1"
    
    plan = create_mock_plan(plan_id, session_id, expired=True)
    preview_plan_store.save(plan)
    
    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": True
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error_type") == "plan_expired"
    assert result["previewPayload"]["status"] == "plan_expired"

def test_apply_revision_conflict_returns_error():
    """요청의 공장 리비전 번호가 저장된 플랜의 리비전과 다를 때 revision_conflict 에러가 리턴되는지 검증합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-revision"
    session_id = "session-1"
    
    plan = create_mock_plan(plan_id, session_id, revision=10)
    preview_plan_store.save(plan)
    
    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 11,
            "approval": True
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error_type") == "revision_conflict"
    assert result["previewPayload"]["status"] == "revision_conflict"

def test_apply_uses_context_factory_revision_when_payload_revision_is_missing():
    """Apply requests also accept factoryRevision from the WebSocket context envelope."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-context-revision"
    session_id = "session-1"

    plan = create_mock_plan(plan_id, session_id, revision=10)
    preview_plan_store.save(plan)

    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "approval": True,
            "approved_change_ids": ["change-02"],
        },
        "context": {
            "factoryRevision": 10,
        },
    }

    result = graph.invoke(initial_state)
    assert result.get("error") is None
    assert result["previewPayload"]["status"] == "execute_ready"
    assert result["previewPayload"]["factoryRevision"] == 10

def test_apply_empty_approved_change_ids_returns_error():
    """An explicit empty approved_change_ids list must not execute all or no changes silently."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-empty-change-selection"
    session_id = "session-1"

    plan = create_mock_plan(plan_id, session_id, revision=10)
    preview_plan_store.save(plan)

    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": [],
        }
    }

    result = graph.invoke(initial_state)
    assert result.get("error_type") == "no_changes_selected"
    assert result["previewPayload"]["status"] == "no_changes_selected"

def test_apply_invalid_change_id_returns_error():
    """프리뷰 제안에 존재하지 않는 잘못된 change_id를 승인하려고 할 때 invalid_change_id 에러가 리턴되는지 검증합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-change-id"
    session_id = "session-1"
    
    plan = create_mock_plan(plan_id, session_id)
    preview_plan_store.save(plan)
    
    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": ["change-01", "non-existent-change"]
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error_type") == "invalid_change_id"
    assert result["previewPayload"]["status"] == "invalid_change_id"

def test_apply_partial_changes_succeeds():
    """일부 항목만 승인하여 적용을 요청했을 때, 승인된 항목들만 approved_changes 및 commands로 안전하게 생성되는지 검증합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-partial"
    session_id = "session-1"
    
    plan = create_mock_plan(plan_id, session_id)
    preview_plan_store.save(plan)
    
    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": ["change-02"]  # change-02 만 승인
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error") is None
    
    preview = result.get("previewPayload")
    assert preview["status"] == "execute_ready"
    assert len(preview["changes"]) == 2
    
    # 승인된 제안 검증
    approved = preview["approved_changes"]
    assert len(approved) == 1
    assert approved[0]["id"] == "change-02"
    
    # 생성된 명령 검증
    commands = preview["commands"]
    assert len(commands) == 1
    assert commands[0]["command"] == "connect_conveyor"
    assert commands[0]["conveyor_id"] == "constructor_01"

def test_apply_execution_record_marks_before_after_as_unconfirmed():
    """Sprint 5 stores planned commands, not guessed world state, in execution records."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-record-state-source"
    session_id = "session-1"

    plan = create_mock_plan(plan_id, session_id)
    preview_plan_store.save(plan)

    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": ["change-02"]
        }
    }

    result = graph.invoke(initial_state)
    assert result.get("error") is None

    record = execution_record_store.get_record(plan_id, "change-02")
    assert record is not None
    assert record.before["state_known"] is False
    assert record.before["source"] == "unreal_runtime_required"
    assert record.after["state_known"] is False
    assert record.after["source"] == "planned_command"
    assert record.after["requires_unreal_confirmation"] is True
    assert record.after["planned_command"]["command"] == "connect_conveyor"

def test_apply_records_explicit_unreal_before_and_after_snapshots():
    """Unreal-provided before/after snapshots are stored ahead of fallback guessed state."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-explicit-snapshots"
    session_id = "session-1"

    plan = create_mock_plan(plan_id, session_id)
    preview_plan_store.save(plan)

    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": ["change-02"],
            "before_states": {
                "change-02": {
                    "id": "constructor_01",
                    "type": "constructor",
                    "status": "operating",
                    "recipe_id": "iron_plate",
                    "before_target": "storage_01",
                    "after_target": "storage_02",
                }
            },
            "after_states": {
                "change-02": {
                    "id": "constructor_01",
                    "type": "constructor",
                    "status": "operating",
                    "recipe_id": "iron_plate",
                    "before_target": "storage_01",
                    "after_target": "smelter_01",
                }
            },
        }
    }

    result = graph.invoke(initial_state)
    assert result.get("error") is None

    record = execution_record_store.get_record(plan_id, "change-02")
    assert record is not None
    assert record.before["state_known"] is True
    assert record.before["source"] == "unreal_pre_apply_snapshot"
    assert record.before["recipe_id"] == "iron_plate"
    assert record.before["after_target"] == "storage_02"
    assert record.after["state_known"] is True
    assert record.after["source"] == "unreal_post_apply_snapshot"
    assert record.after["after_target"] == "smelter_01"
    assert record.after["planned_command"]["command"] == "connect_conveyor"

def test_pipeline_apply_uses_v2_graph_and_records_authoritative_before_state():
    """The public AgentPipeline routes apply requests into the v2 graph."""
    pipeline = AgentPipeline()
    plan_id = "plan-pipeline-apply"
    session_id = "session-pipeline-apply"

    plan = create_mock_plan(plan_id, session_id)
    preview_plan_store.save(plan)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-pipeline-apply",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": ["change-02"],
            "factory_state": {
                "machines": [
                    {
                        "id": "constructor_01",
                        "type": "constructor",
                        "status": "operating",
                        "recipe_id": "iron_plate",
                    }
                ],
                "conveyors": [],
                "power_grid": {"produced": 100, "consumed": 50},
            },
        },
    }

    result = pipeline.run(request_msg)

    assert result["type"] == "agent.response"
    payload = result["payload"]
    assert payload["status"] == "execute_ready"
    assert payload["commands"][0]["command"] == "connect_conveyor"

    record = execution_record_store.get_record(plan_id, "change-02")
    assert record is not None
    assert record.before["state_known"] is True
    assert record.before["recipe_id"] == "iron_plate"

def test_apply_duplicate_execution_fails():
    """이미 적용 완료된 plan_id와 change_id로 중복 요청 시 duplicate_execution 에러로 차단되는지 검증합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-duplicate"
    session_id = "session-1"
    
    plan = create_mock_plan(plan_id, session_id)
    preview_plan_store.save(plan)
    
    initial_state = {
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "session_id": session_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": ["change-01"]
        }
    }
    
    # 1차 실행 요청
    result1 = graph.invoke(initial_state)
    assert result1.get("error") is None
    assert result1["previewPayload"]["status"] == "execute_ready"
    
    # 2차 중복 실행 요청
    result2 = graph.invoke(initial_state)
    assert result2.get("error_type") == "duplicate_execution"
    assert result2["previewPayload"]["status"] == "duplicate_execution"
