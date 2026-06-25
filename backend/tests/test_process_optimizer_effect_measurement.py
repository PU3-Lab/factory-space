"""Unit tests for Process Optimizer LangGraph v2 Effect Measurement Flow.

초보자 설명:
이 테스트 모듈은 최적화 계획이 적용된 이후, 실제 효과(성능)를 측정하는 measure 흐름을 검증합니다.
시간 부족, 생산 주기 부족으로 인한 측정 준비 미달(measurement_not_ready),
개선 성공(success), 개선 미달(failed), 그리고 적용 후 오히려 나빠져 재분석을 제안하는 악화(degraded) 상태를 분석하고,
악화 시에도 안전상 자동으로 되돌리기 명령어가 생성되지 않는지 여부를 모두 단언(assert)합니다.
"""

from datetime import datetime, timedelta, timezone
import pytest
from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.schemas import PreviewPlan, UiHints, OptimizationSuggestion, TargetDescriptor
from agents.process_optimizer.preview_store import preview_plan_store
from agents.process_optimizer.execution_record import ExecutionRecord, execution_record_store
from agents.process_optimizer.graph import compile_process_optimizer_graph

@pytest.fixture(autouse=True)
def clean_stores():
    """각 테스트 격리성을 위해 저장소들을 비워줍니다."""
    preview_plan_store.clear()
    execution_record_store.clear()

def create_mock_preview_plan(plan_id: str, session_id: str) -> PreviewPlan:
    """테스트용 예상 효과가 포함된 Mock 프리뷰 계획을 생성합니다.
    예상 해소 건수: input_shortage 1건 해소 기대
    """
    changes = [
        OptimizationSuggestion(
            id="change-1",
            target=TargetDescriptor(type="machine", id="smelter_01"),
            problem="Input is low.",
            recommended_action="Set recipe to iron_ingot and turn on.",
            expected_effect="Resolve input shortage."
        )
    ]
    return PreviewPlan(
        plan_id=plan_id,
        session_id=session_id,
        factoryRevision=10,
        goal="balance",
        changes=changes,
        expected_effect={
            "resolved_input_shortages_count": 1,
            "resolved_output_blocks_count": 0,
            "resolved_conveyor_congestions_count": 0
        },
        ui_hints=UiHints(highlight_targets=["smelter_01"]),
        created_at=datetime.now(timezone.utc),
        expires_at=datetime.now(timezone.utc) + timedelta(minutes=5)
    )

def test_measurement_not_ready_due_to_time():
    """적용 후 30초 미만 경과 시 measurement_not_ready를 반환하는지 테스트합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-measure-time"
    session_id = "session-1"
    
    # 1. 10초 전에 적용되었다고 ExecutionRecord 생성
    applied_time = datetime.now(timezone.utc) - timedelta(seconds=10)
    rec = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-1",
        before={"recipe_id": "copper_ingot", "enabled": False},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {"command": "set_recipe", "machine_id": "smelter_01", "recipe_id": "iron_ingot"}
        },
        revision=10,
        created_at=applied_time
    )
    execution_record_store.save(rec)
    
    # 2. measure 요청 (생산주기 5회 완료로 만족함)
    initial_state = {
        "payload": {
            "operation": "measure",
            "plan_id": plan_id,
            "production_cycles": 5,
            "current_time": datetime.now(timezone.utc).isoformat(),
            "factory_state": {
                "machines": [{"id": "smelter_01", "type": "smelter", "status": "operating", "recipe_id": "iron_ingot"}],
                "conveyors": []
            }
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error_type") == "measurement_not_ready"
    assert result["previewPayload"]["status"] == "measurement_not_ready"
    assert "Observation duration is insufficient" in result["previewPayload"]["summary"]

def test_measurement_not_ready_due_to_cycles():
    """적용 후 40초 경과했지만 생산주기가 2회로 부족한 경우 measurement_not_ready를 반환하는지 테스트합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-measure-cycles"
    
    # 1. 40초 전에 적용됨 (시간 만족)
    applied_time = datetime.now(timezone.utc) - timedelta(seconds=40)
    rec = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-1",
        before={"recipe_id": "copper_ingot", "enabled": False},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {"command": "set_recipe", "machine_id": "smelter_01", "recipe_id": "iron_ingot"}
        },
        revision=10,
        created_at=applied_time
    )
    execution_record_store.save(rec)
    
    # 2. 생산주기가 2회인 measure 요청
    initial_state = {
        "payload": {
            "operation": "measure",
            "plan_id": plan_id,
            "production_cycles": 2, # 3 미만
            "current_time": datetime.now(timezone.utc).isoformat(),
            "factory_state": {
                "machines": [{"id": "smelter_01", "type": "smelter", "status": "operating", "recipe_id": "iron_ingot"}],
                "conveyors": []
            }
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error_type") == "measurement_not_ready"
    assert result["previewPayload"]["status"] == "measurement_not_ready"
    assert "production cycles" in result["previewPayload"]["summary"]

def test_measurement_success():
    """예상했던 개선(input_shortage 해소)이 성공적으로 완료되었을 때 status: success가 리턴되는지 테스트합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-success"
    session_id = "default-session"
    
    # 1. Mock 프리뷰 계획 저장 (예상 resolved_input_shortages_count: 1)
    plan = create_mock_preview_plan(plan_id, session_id)
    preview_plan_store.save(plan)
    
    # 2. 실행 기록 저장 (40초 전 적용)
    # 원래 상태(before)에는 기계가 가동 중이지만 수량이 0이어서 input shortage가 있던 상황을 기록
    applied_time = datetime.now(timezone.utc) - timedelta(seconds=40)
    rec = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-1",
        before={
            "enabled": True, 
            "operating_rate": 0.5,
            "inputs": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}] # shortage 유발!
        },
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {"command": "set_machine_enabled", "machine_id": "smelter_01", "enabled": True}
        },
        revision=10,
        created_at=applied_time
    )
    execution_record_store.save(rec)
    
    # 3. 현재 공장 상태 구성 (smelter_01은 가동률 1.0에 input이 10개로 부족하지 않음 -> shortage 해결됨)
    factory_state = {
        "machines": [
            {
                "id": "smelter_01",
                "type": "smelter",
                "status": "operating",
                "operating_rate": 1.0,
                "inputs": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 100.0}],
                "outputs": []
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100, "consumed": 50}
    }
    
    initial_state = {
        "payload": {
            "operation": "measure",
            "plan_id": plan_id,
            "production_cycles": 5,
            "current_time": datetime.now(timezone.utc).isoformat(),
            "factory_state": factory_state
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error") is None
    
    payload = result.get("previewPayload")
    assert payload["status"] == "measurement_ready"
    
    report = payload["measurement_result"]
    assert report["status"] == "success"
    assert report["next_action"] == "monitor"
    assert report["actual_effect"]["resolved_input_shortages_count"] == 1
    assert not payload.get("commands")

def test_measurement_degraded_reanalyze():
    """최적화 적용 후 가동률이 하락하여 악화된 경우 reanalyze 상태를 반환하고 자동 undo가 없는지 테스트합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-degraded"
    session_id = "default-session"
    
    plan = create_mock_preview_plan(plan_id, session_id)
    preview_plan_store.save(plan)
    
    # 1. 40초 전 적용 기록 저장
    # 원래 상태(before)에는 기계가 가동률 1.0으로 잘 가동되고 있었음
    applied_time = datetime.now(timezone.utc) - timedelta(seconds=40)
    rec = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-1",
        before={
            "enabled": True, 
            "operating_rate": 1.0,
            "inputs": [{"item_id": "iron_ore", "amount": 5.0, "max_amount": 100.0}]
        },
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {"command": "set_machine_enabled", "machine_id": "smelter_01", "enabled": True}
        },
        revision=10,
        created_at=applied_time
    )
    execution_record_store.save(rec)
    
    # 2. 현재 상태: smelter_01의 status가 'disabled'로 꺼지고 가동률이 0.0이 됨 (악화)
    factory_state = {
        "machines": [
            {
                "id": "smelter_01",
                "type": "smelter",
                "status": "disabled",
                "operating_rate": 0.0,
                "inputs": [],
                "outputs": []
            }
        ],
        "conveyors": []
    }
    
    initial_state = {
        "payload": {
            "operation": "measure",
            "plan_id": plan_id,
            "production_cycles": 4,
            "current_time": datetime.now(timezone.utc).isoformat(),
            "factory_state": factory_state
        }
    }
    
    result = graph.invoke(initial_state)
    assert result.get("error") is None
    
    payload = result.get("previewPayload")
    assert payload["status"] == "measurement_ready"
    
    report = payload["measurement_result"]
    assert report["status"] == "degraded"
    assert report["next_action"] == "reanalyze"
    assert not payload.get("commands")

def test_measurement_unknown_before_state_is_not_ready():
    """Authoritative before state is required before classifying measurement results."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-measure-unknown-before"
    applied_time = datetime.now(timezone.utc) - timedelta(seconds=40)

    record = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-unknown-before",
        before={
            "state_known": False,
            "source": "unreal_runtime_required",
            "target": {"type": "machine", "id": "smelter_01"},
        },
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_machine_enabled",
                "machine_id": "smelter_01",
                "enabled": True,
            },
        },
        revision=10,
        created_at=applied_time,
    )
    execution_record_store.save(record)

    result = graph.invoke(
        {
            "payload": {
                "operation": "measure",
                "plan_id": plan_id,
                "production_cycles": 4,
                "current_time": datetime.now(timezone.utc).isoformat(),
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_01",
                            "type": "smelter",
                            "status": "operating",
                            "operating_rate": 1.0,
                        }
                    ],
                    "conveyors": [],
                },
            }
        }
    )

    assert result.get("error_type") == "measurement_not_ready"
    assert result["previewPayload"]["status"] == "measurement_not_ready"
    assert "authoritative before state" in result["previewPayload"]["summary"]

def test_pipeline_measure_routes_to_v2_graph():
    """The public AgentPipeline routes measure requests into the v2 graph."""
    pipeline = AgentPipeline()
    plan_id = "plan-pipeline-measure"
    session_id = "session-pipeline-measure"

    plan = create_mock_preview_plan(plan_id, session_id)
    preview_plan_store.save(plan)

    record = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-1",
        before={
            "enabled": True,
            "operating_rate": 0.5,
            "inputs": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}],
        },
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_machine_enabled",
                "machine_id": "smelter_01",
                "enabled": True,
            },
        },
        revision=10,
        created_at=datetime.now(timezone.utc) - timedelta(seconds=40),
    )
    execution_record_store.save(record)

    result = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-pipeline-measure",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "measure",
                "plan_id": plan_id,
                "production_cycles": 5,
                "current_time": datetime.now(timezone.utc).isoformat(),
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_01",
                            "type": "smelter",
                            "status": "operating",
                            "operating_rate": 1.0,
                            "inputs": [
                                {
                                    "item_id": "iron_ore",
                                    "amount": 10.0,
                                    "max_amount": 100.0,
                                }
                            ],
                            "outputs": [],
                        }
                    ],
                    "conveyors": [],
                    "power_grid": {"produced": 100, "consumed": 50},
                },
            },
        }
    )

    assert result["type"] == "agent.response"
    payload = result["payload"]
    assert payload["status"] == "measurement_ready"
    assert payload["measurement_result"]["status"] == "success"
    assert payload["commands"] == []
