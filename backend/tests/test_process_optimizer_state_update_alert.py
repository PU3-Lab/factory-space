"""Unit and integration tests for Process Optimizer Subquest Sprint 1 State Update Alert."""

from __future__ import annotations

from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
from agents.process_optimizer.schemas import (
    ConveyorState,
    FactoryState,
    InventoryItem,
    MachineState,
    PowerGridState,
)
from agents.process_optimizer.session_memory import process_optimizer_memory
from agents.process_optimizer.subquest_alert import SubquestAlertBuilder


def test_alert_builder_direct_normal() -> None:
    """정상 공장 상태에서 alert.needed=False 가 반환되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()
    builder = SubquestAlertBuilder()

    state = FactoryState(
        machines=[
            MachineState(
                id="smelter_1",
                type="smelter",
                status="operating",
                inputs=[InventoryItem(item_id="iron_ore", amount=10.0)],
                outputs=[InventoryItem(item_id="iron_ingot", amount=5.0)],
            )
        ],
        conveyors=[ConveyorState(id="conv_1", congestion_rate=0.2)],
        power_grid=PowerGridState(produced=100.0, consumed=15.0),
    )

    report = analyzer.analyze(state, factory_revision=1, goal="balance")
    alert = builder.build_alert(report, state, subquest_mode=True)

    assert alert.needed is False


def test_alert_builder_direct_input_shortage() -> None:
    """입력 부족 장비가 포함되었을 때 medium alert와 올바른 서브퀘스트 정보가 생성되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()
    builder = SubquestAlertBuilder()

    state = FactoryState(
        machines=[
            MachineState(
                id="smelter_1",
                type="smelter",
                status="operating",
                inputs=[InventoryItem(item_id="iron_ore", amount=0.0)],
                outputs=[InventoryItem(item_id="iron_ingot", amount=5.0)],
            )
        ],
        storages=[
            {
                "id": "storage_1",
                "inventory": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 100.0}]
            }
        ]
    )

    report = analyzer.analyze(state, factory_revision=1, goal="balance")
    alert = builder.build_alert(report, state, subquest_mode=True)

    assert alert.needed is True
    assert alert.severity == "medium"
    assert "smelter_1" in alert.reason
    assert alert.target is not None
    assert alert.target.type == "machine"
    assert alert.target.id == "smelter_1"
    assert alert.suggested_subquest is not None
    assert alert.suggested_subquest.title == "철광석 공급 라인 점검"
    assert "철광석" in alert.suggested_subquest.objective


def test_alert_builder_direct_output_blocked() -> None:
    """출력 적체 장비가 포함되었을 때 medium alert가 생성되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()
    builder = SubquestAlertBuilder()

    state = FactoryState(
        machines=[
            MachineState(
                id="constructor_1",
                type="constructor",
                status="operating",
                inputs=[InventoryItem(item_id="iron_ingot", amount=10.0)],
                outputs=[
                    InventoryItem(item_id="iron_plate", amount=100.0, max_amount=100.0)
                ],
            )
        ]
    )

    report = analyzer.analyze(state, factory_revision=1, goal="balance")
    alert = builder.build_alert(report, state, subquest_mode=True)

    assert alert.needed is True
    assert alert.severity == "medium"
    assert "constructor_1" in alert.reason
    assert alert.target is not None
    assert alert.target.id == "constructor_1"
    assert alert.suggested_subquest is not None
    assert alert.suggested_subquest.title == "제작기 출력 라인 적체 해소"


def test_alert_builder_direct_power_issue() -> None:
    """전력 소비량이 생산량을 초과할 때 high alert가 생성되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()
    builder = SubquestAlertBuilder()

    state = FactoryState(
        power_grid={
            "produced": 50.0,
            "consumed": 60.0,
            "nodes": [{"id": "pole_1", "type": "power_pole", "connected_node_ids": []}],
            "generators": [{"id": "generator_1", "produced": 50.0, "connected": True, "connected_power_node_ids": ["pole_1"]}]
        }
    )

    report = analyzer.analyze(state, factory_revision=1, goal="balance")
    alert = builder.build_alert(report, state, subquest_mode=True)

    assert alert.needed is True
    assert alert.severity == "high"
    assert "전력" in alert.reason
    assert alert.target is None
    assert alert.suggested_subquest is not None
    assert alert.suggested_subquest.title == "전력 공급망 복구"


def test_alert_builder_direct_conveyor_congestion() -> None:
    """컨베이어 혼잡도 0.8 이상일 때 conveyor target alert가 생성되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()
    builder = SubquestAlertBuilder()

    # goal: balance -> low severity
    state = FactoryState(conveyors=[ConveyorState(id="conv_1", congestion_rate=0.85)])
    report = analyzer.analyze(state, factory_revision=1, goal="balance")
    alert = builder.build_alert(report, state, subquest_mode=True)

    assert alert.needed is True
    assert alert.severity == "low"
    assert alert.target is not None
    assert alert.target.id == "conv_1"
    assert alert.suggested_subquest is not None
    assert alert.suggested_subquest.title == "컨베이어 이송 정체 해소"

    # goal: congestion_relief -> medium severity
    report_relief = analyzer.analyze(
        state, factory_revision=1, goal="congestion_relief"
    )
    alert_relief = builder.build_alert(report_relief, state, subquest_mode=True)
    assert alert_relief.severity == "medium"


def test_alert_builder_direct_subquest_mode_disabled() -> None:
    """subquest_mode=False 일 때 alert.needed=False 가 반환되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()
    builder = SubquestAlertBuilder()

    state = FactoryState(power_grid=PowerGridState(produced=50.0, consumed=60.0))

    report = analyzer.analyze(state, factory_revision=1, goal="balance")
    alert = builder.build_alert(report, state, subquest_mode=False)

    assert alert.needed is False


def test_pipeline_state_update_alert_integration() -> None:
    """AgentPipeline을 통해 state_update 요청 시 최적화 알림이 응답에 포함되는지 검증합니다."""
    pipeline = AgentPipeline()
    session_id = "session-state-update-alert"
    process_optimizer_memory.clear(session_id)

    # 전력 부족 시나리오
    request_msg = {
        "type": "agent.request",
        "request_id": "req-state-alert-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "balance",
            "factoryRevision": 150,
            "subquest_mode": True,
            "factory_state": {
                "machines": [],
                "conveyors": [],
                "power_grid": {
                    "produced": 100.0,
                    "consumed": 120.0,
                    "nodes": [{"id": "pole_1", "type": "power_pole", "connected_node_ids": []}],
                    "generators": [{"id": "generator_1", "produced": 100.0, "connected": True, "connected_power_node_ids": ["pole_1"]}]
                },
            },
        },
    }

    res = pipeline.run(request_msg)

    assert res.get("type") == "agent.response"
    assert res["payload"]["status"] == "success"
    assert res["payload"]["factoryRevision"] == 150

    alert_data = res["payload"]["optimization_alert"]
    assert alert_data["needed"] is True
    assert alert_data["severity"] == "high"
    assert alert_data["suggested_subquest"]["title"] == "전력 공급망 복구"

    # state_update 응답에 commands, plan_id, changes가 포함되지 않아야 함
    assert "commands" not in res["payload"]
    assert "plan_id" not in res["payload"]
    assert "changes" not in res["payload"]


def test_pipeline_state_update_subquest_mode_disabled() -> None:
    """subquest_mode=False 일 때 pipeline 응답에서 needed=False 임을 검증합니다."""
    pipeline = AgentPipeline()
    session_id = "session-state-update-disabled"
    process_optimizer_memory.clear(session_id)

    # 입력 부족 시나리오 + subquest_mode=False
    request_msg = {
        "type": "agent.request",
        "request_id": "req-state-alert-disabled",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "balance",
            "factoryRevision": 150,
            "subquest_mode": False,
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                    }
                ],
            },
        },
    }

    res = pipeline.run(request_msg)

    assert res.get("type") == "agent.response"
    alert_data = res["payload"]["optimization_alert"]
    assert alert_data["needed"] is False
