"""Unit tests for Process Optimizer LangGraph v2 Undo Flow.

초보자 설명:
이 테스트 모듈은 플레이어의 최적화 실행 되돌리기(Undo) 흐름을 검증합니다.
성공 시나리오(상태 일치), 충돌 시나리오(플레이어가 직접 상태 수정함), 기록 없음 시나리오,
그리고 여러 변경 중 하나라도 충돌나면 전체 복구가 차단되는 트랜잭션 일체형 흐름을 시뮬레이션하고 검사합니다.
"""

import pytest
from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.schemas import FactoryState, MachineState, ConveyorState
from agents.process_optimizer.execution_record import ExecutionRecord, execution_record_store
from agents.process_optimizer.graph import compile_process_optimizer_graph

@pytest.fixture(autouse=True)
def clean_records():
    """각 테스트 간의 격리성을 위해 실행 기록 저장소를 비워줍니다."""
    execution_record_store.clear()

def test_undo_success_recipe():
    """현재 상태가 recorded after와 일치할 때, 원래의 before 상태(레시피)로 되돌리는 inverse command가 올바르게 생성되는지 테스트합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-undo-001"
    
    # 1. 실행 기록 저장: 적용 후 레시피가 'iron_ingot', 적용 전 레시피가 'copper_ingot'이었던 기록
    record = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-recipe",
        before={"recipe_id": "copper_ingot", "enabled": True},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_recipe",
                "machine_id": "smelter_01",
                "recipe_id": "iron_ingot"
            }
        },
        revision=5
    )
    execution_record_store.save(record)
    
    # 2. 현재 공장 상태 구성: 기계 smelter_01의 레시피가 'iron_ingot'임 (after 상태와 정확히 일치)
    factory_state = {
        "machines": [
            {
                "id": "smelter_01",
                "type": "smelter",
                "status": "operating",
                "recipe_id": "iron_ingot"
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100, "consumed": 50}
    }
    
    initial_state = {
        "payload": {
            "operation": "undo",
            "plan_id": plan_id,
            "factory_state": factory_state
        }
    }
    
    result = graph.invoke(initial_state)
    
    # 3. 검증
    assert result.get("error") is None
    payload = result.get("previewPayload")
    assert payload["status"] == "undo_ready"
    assert payload["plan_id"] == plan_id
    assert payload["expected_effect"] == {"estimated": False}
    
    # 원래 상태인 'copper_ingot'으로 복구하는 inverse command가 반환되어야 함
    commands = payload["commands"]
    assert len(commands) == 1
    assert commands[0]["command"] == "set_recipe"
    assert commands[0]["machine_id"] == "smelter_01"
    assert commands[0]["recipe_id"] == "copper_ingot"

def test_pipeline_undo_routes_to_v2_graph():
    """The public AgentPipeline routes undo requests into the v2 graph."""
    pipeline = AgentPipeline()
    plan_id = "plan-pipeline-undo"

    record = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-recipe",
        before={"recipe_id": "copper_ingot", "enabled": True},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_recipe",
                "machine_id": "smelter_01",
                "recipe_id": "iron_ingot",
            },
        },
        revision=5,
    )
    execution_record_store.save(record)

    result = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-pipeline-undo",
            "session_id": "session-pipeline-undo",
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "undo",
                "plan_id": plan_id,
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_01",
                            "type": "smelter",
                            "status": "operating",
                            "recipe_id": "iron_ingot",
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
    assert payload["status"] == "undo_ready"
    assert payload["commands"][0]["command"] == "set_recipe"
    assert payload["commands"][0]["recipe_id"] == "copper_ingot"

def test_undo_unknown_before_state_blocks_inverse_command():
    """Undo must not guess a before value when Sprint 5 recorded it as unknown."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-undo-unknown-before"

    record = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-recipe",
        before={
            "state_known": False,
            "source": "unreal_runtime_required",
            "target": {"type": "machine", "id": "smelter_01"},
        },
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_recipe",
                "machine_id": "smelter_01",
                "recipe_id": "iron_ingot",
            },
        },
        revision=5,
    )
    execution_record_store.save(record)

    result = graph.invoke(
        {
            "payload": {
                "operation": "undo",
                "plan_id": plan_id,
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_01",
                            "type": "smelter",
                            "status": "operating",
                            "recipe_id": "iron_ingot",
                        }
                    ],
                    "conveyors": [],
                    "power_grid": {"produced": 100, "consumed": 50},
                },
            }
        }
    )

    assert result.get("error_type") == "undo_conflict"
    payload = result["previewPayload"]
    assert payload["status"] == "undo_conflict"
    assert payload["commands"] == []

def test_undo_conflict_when_user_modified_state():
    """적용 완료된 후(after) 상태와 현재 공장 상태가 다르면(플레이어 직접 수정), 되돌리기를 중단하고 undo_conflict 에러를 반환하는지 테스트합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-undo-002"
    
    # 1. 실행 기록 저장: 적용 후 레시피가 'iron_ingot'이어야 함
    record = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-recipe",
        before={"recipe_id": "copper_ingot", "enabled": True},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_recipe",
                "machine_id": "smelter_01",
                "recipe_id": "iron_ingot"
            }
        },
        revision=5
    )
    execution_record_store.save(record)
    
    # 2. 현재 공장 상태 구성: smelter_01의 레시피를 플레이어가 직접 'steel_ingot'으로 수정한 상황
    factory_state = {
        "machines": [
            {
                "id": "smelter_01",
                "type": "smelter",
                "status": "operating",
                "recipe_id": "steel_ingot" # 'iron_ingot'이 아니므로 불일치(충돌)
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100, "consumed": 50}
    }
    
    initial_state = {
        "payload": {
            "operation": "undo",
            "plan_id": plan_id,
            "factory_state": factory_state
        }
    }
    
    result = graph.invoke(initial_state)
    
    # 3. 검증
    assert result.get("error_type") == "undo_conflict"
    payload = result.get("previewPayload")
    assert payload["status"] == "undo_conflict"
    assert "Undo conflict detected" in payload["summary"]
    # 충돌 시 command 페이로드가 비어있어야 함
    assert not payload.get("commands")

def test_undo_uses_authoritative_after_snapshot_before_planned_command():
    """Unreal이 기록한 after snapshot이 있으면 그 값을 기준으로 직접 수정 충돌을 판단합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-undo-authoritative-after"

    record = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-recipe",
        before={"state_known": True, "recipe_id": "copper_ingot", "enabled": True},
        after={
            "state_known": True,
            "source": "unreal_post_apply_snapshot",
            "id": "smelter_01",
            "type": "smelter",
            "status": "operating",
            "recipe_id": "iron_ingot",
            "planned_command": {
                "command": "set_recipe",
                "machine_id": "smelter_01",
                "recipe_id": "iron_ingot",
            },
        },
        revision=5,
    )
    execution_record_store.save(record)

    result = graph.invoke(
        {
            "payload": {
                "operation": "undo",
                "plan_id": plan_id,
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_01",
                            "type": "smelter",
                            "status": "operating",
                            "recipe_id": "steel_ingot",
                        }
                    ],
                    "conveyors": [],
                    "power_grid": {"produced": 100, "consumed": 50},
                },
            }
        }
    )

    assert result.get("error_type") == "undo_conflict"
    assert result["previewPayload"]["commands"] == []

def test_undo_record_not_found():
    """저장소에 실행 기록이 없는 plan_id로 되돌리기를 요청하면 record_not_found 에러를 반환하는지 테스트합니다."""
    graph = compile_process_optimizer_graph()
    
    factory_state = {
        "machines": [
            {"id": "smelter_01", "type": "smelter", "status": "operating"}
        ],
        "conveyors": []
    }
    
    initial_state = {
        "payload": {
            "operation": "undo",
            "plan_id": "non-existent-plan",
            "factory_state": factory_state
        }
    }
    
    result = graph.invoke(initial_state)
    
    assert result.get("error_type") == "record_not_found"
    payload = result.get("previewPayload")
    assert payload["status"] == "record_not_found"
    assert "were not found" in payload["summary"]

def test_undo_all_or_nothing_transaction():
    """여러 변경 사항 중 일부만 충돌이 일어나더라도, 전체 플랜 복구를 원천 중단하는 트랜잭션 성질을 테스트합니다."""
    graph = compile_process_optimizer_graph()
    plan_id = "plan-undo-multi"
    
    # 1. 2개의 변경 기록 저장
    # change-1: smelter_01 가동 설정 (정상)
    rec1 = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-1",
        before={"enabled": False},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_machine_enabled",
                "machine_id": "smelter_01",
                "enabled": True
            }
        },
        revision=5
    )
    # change-2: constructor_01 레시피 설정 (충돌 예정)
    rec2 = ExecutionRecord(
        plan_id=plan_id,
        change_id="change-2",
        before={"recipe_id": "iron_plate"},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {
                "command": "set_recipe",
                "machine_id": "constructor_01",
                "recipe_id": "iron_gear"
            }
        },
        revision=5
    )
    execution_record_store.save(rec1)
    execution_record_store.save(rec2)
    
    # 2. 현재 상태: smelter_01은 'enabled=True' (일치), constructor_01은 'steel_gear'로 플레이어가 변경함 (충돌)
    factory_state = {
        "machines": [
            {
                "id": "smelter_01",
                "type": "smelter",
                "status": "operating" # status != "disabled" 이므로 enabled=True로 매핑됨
            },
            {
                "id": "constructor_01",
                "type": "constructor",
                "status": "operating",
                "recipe_id": "steel_gear" # 'iron_gear'가 아니므로 충돌
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100, "consumed": 50}
    }
    
    initial_state = {
        "payload": {
            "operation": "undo",
            "plan_id": plan_id,
            "factory_state": factory_state
        }
    }
    
    result = graph.invoke(initial_state)
    
    # 3. 검증
    assert result.get("error_type") == "undo_conflict"
    payload = result.get("previewPayload")
    assert payload["status"] == "undo_conflict"
    assert "Undo conflict detected" in payload["summary"]
    # 단 하나라도 충돌이 났기 때문에 inverse command는 일절 생성되지 않아야 함
    assert not payload.get("commands")
