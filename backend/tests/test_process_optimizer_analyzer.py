"""Unit tests for FactoryStateAnalyzerTool."""

from __future__ import annotations

import pytest

from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
from agents.process_optimizer.schemas import (
    ConveyorState,
    FactoryState,
    InventoryItem,
    MachineState,
    PowerGridState,
)


def test_analyzer_normal_factory() -> None:
    """정상적으로 운용 중이고 병목이 없는 공장 상태를 분석합니다."""
    analyzer = FactoryStateAnalyzerTool()

    state = FactoryState(
        machines=[
            MachineState(
                id="smelter_1",
                type="smelter",
                status="operating",
                operating_rate=0.9,
                inputs=[
                    InventoryItem(item_id="iron_ore", amount=10.0, max_amount=100.0)
                ],
                outputs=[
                    InventoryItem(item_id="iron_ingot", amount=5.0, max_amount=100.0)
                ],
                power_consumption=10.0,
            ),
            MachineState(
                id="constructor_1",
                type="constructor",
                status="operating",
                operating_rate=0.8,
                inputs=[
                    InventoryItem(item_id="iron_ingot", amount=15.0, max_amount=100.0)
                ],
                outputs=[
                    InventoryItem(item_id="iron_plate", amount=2.0, max_amount=100.0)
                ],
                power_consumption=5.0,
            ),
        ],
        conveyors=[
            ConveyorState(id="conv_1", congestion_rate=0.1),
            ConveyorState(id="conv_2", congestion_rate=0.4),
        ],
        power_grid=PowerGridState(produced=100.0, consumed=15.0),
    )

    report = analyzer.analyze(state, factory_revision=12, goal="balance")

    assert report.factoryRevision == 12
    assert report.goal == "balance"
    assert report.average_operating_rate == pytest.approx(0.85)
    assert len(report.input_shortages) == 0
    assert len(report.output_blocked) == 0
    assert len(report.congested_conveyors) == 0
    assert report.average_conveyor_congestion == pytest.approx(0.25)
    assert report.power_summary.produced == 100.0
    assert report.power_summary.consumed == 15.0
    assert report.power_summary.power_issue is False


def test_analyzer_input_shortage() -> None:
    """특정 장비의 입력 재고가 0인 경우 input_shortages로 감지되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()

    state = FactoryState(
        machines=[
            # 철광석 재고가 0이므로 부족 상태
            MachineState(
                id="smelter_1",
                type="smelter",
                status="operating",
                operating_rate=0.2,
                inputs=[
                    InventoryItem(item_id="iron_ore", amount=0.0, max_amount=100.0)
                ],
                outputs=[
                    InventoryItem(item_id="iron_ingot", amount=10.0, max_amount=100.0)
                ],
            ),
            # 구리광석 재고가 5.0으로 정상이어서 감지되지 않아야 함
            MachineState(
                id="smelter_2",
                type="smelter",
                status="operating",
                operating_rate=0.9,
                inputs=[
                    InventoryItem(item_id="copper_ore", amount=5.0, max_amount=100.0)
                ],
                outputs=[
                    InventoryItem(item_id="copper_ingot", amount=0.0, max_amount=100.0)
                ],
            ),
            # 비활성화(disabled)된 장비는 입력 재고가 0이어도 부족 감지 대상에서 제외되어야 함
            MachineState(
                id="smelter_3",
                type="smelter",
                status="disabled",
                operating_rate=0.0,
                inputs=[
                    InventoryItem(item_id="iron_ore", amount=0.0, max_amount=100.0)
                ],
            ),
        ]
    )

    report = analyzer.analyze(state, factory_revision=5, goal="throughput")

    assert report.factoryRevision == 5
    assert report.goal == "throughput"
    assert "smelter_1" in report.input_shortages
    assert "smelter_2" not in report.input_shortages
    assert "smelter_3" not in report.input_shortages
    assert len(report.input_shortages) == 1


def test_analyzer_output_blocked() -> None:
    """특정 장비의 출력 공간이 가득 찬 경우 output_blocked로 감지되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()

    state = FactoryState(
        machines=[
            # 출력 인벤토리가 가득 참 (100.0 / 100.0) -> blocked
            MachineState(
                id="smelter_1",
                type="smelter",
                status="operating",
                operating_rate=0.1,
                outputs=[
                    InventoryItem(item_id="iron_ingot", amount=100.0, max_amount=100.0)
                ],
            ),
            # 출력 인벤토리가 채워지는 중 (50.0 / 100.0) -> blocked 아님
            MachineState(
                id="smelter_2",
                type="smelter",
                status="operating",
                operating_rate=0.8,
                outputs=[
                    InventoryItem(item_id="copper_ingot", amount=50.0, max_amount=100.0)
                ],
            ),
            # 비활성화(disabled)된 장비는 출력 공간이 가득 차도 blocked 감지 대상에서 제외
            MachineState(
                id="smelter_3",
                type="smelter",
                status="disabled",
                operating_rate=0.0,
                outputs=[
                    InventoryItem(item_id="iron_ingot", amount=100.0, max_amount=100.0)
                ],
            ),
        ]
    )

    report = analyzer.analyze(state, factory_revision=1, goal="balance")

    assert "smelter_1" in report.output_blocked
    assert "smelter_2" not in report.output_blocked
    assert "smelter_3" not in report.output_blocked
    assert len(report.output_blocked) == 1


def test_analyzer_conveyor_congestion() -> None:
    """컨베이어 혼잡도가 0.8 이상인 경우 congested_conveyors로 감지되는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()

    state = FactoryState(
        conveyors=[
            ConveyorState(id="conv_low", congestion_rate=0.2),
            ConveyorState(id="conv_high", congestion_rate=0.85),
            ConveyorState(id="conv_max", congestion_rate=1.0),
        ]
    )

    report = analyzer.analyze(state, factory_revision=10, goal="congestion_relief")

    assert report.average_conveyor_congestion == pytest.approx(0.6833333, abs=1e-5)
    assert "conv_high" in report.congested_conveyors
    assert "conv_max" in report.congested_conveyors
    assert "conv_low" not in report.congested_conveyors
    assert len(report.congested_conveyors) == 2


def test_analyzer_power_issue() -> None:
    """소비 전력이 생산 전력보다 많을 때 power_issue가 발생하는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()

    # 정상 전력 상태
    state_ok = FactoryState(power_grid=PowerGridState(produced=120.0, consumed=100.0))
    report_ok = analyzer.analyze(state_ok, goal="power_saving")
    assert report_ok.power_summary.power_issue is False

    # 전력 부족 상태
    state_fail = FactoryState(power_grid=PowerGridState(produced=100.0, consumed=105.0))
    report_fail = analyzer.analyze(state_fail, goal="power_saving")
    assert report_fail.power_summary.power_issue is True


def test_analyzer_backward_compatibility() -> None:
    """기존 통합 테스트의 뼈대 형태(machines 리스트만 존재하거나 빈 데이터)로 입력되어도 오류 없이 분석이 성공하는지 검증합니다."""
    analyzer = FactoryStateAnalyzerTool()

    # dict 형태로 machines만 간략히 제공되는 케이스
    simple_dict = {
        "machines": [
            {"id": "m1"},
            {"id": "m2"},
        ]
    }

    report = analyzer.analyze(simple_dict, factory_revision=42)

    assert report.factoryRevision == 42
    # MachineState의 default 값들로 파싱되어 동작해야 함
    assert report.average_operating_rate == 0.0
    assert len(report.input_shortages) == 0
    assert len(report.output_blocked) == 0
    assert len(report.congested_conveyors) == 0
    assert report.power_summary.power_issue is False
    assert report.power_summary.produced == 0.0


def test_analyzer_conveyor_congestion_boundary() -> None:
    """Validate conveyor congestion at exactly 0.8 is detected as congested."""
    analyzer = FactoryStateAnalyzerTool()
    state = FactoryState(
        conveyors=[
            ConveyorState(id="conv_border", congestion_rate=0.8),
            ConveyorState(id="conv_below", congestion_rate=0.79),
        ]
    )
    report = analyzer.analyze(state)
    assert "conv_border" in report.congested_conveyors
    assert "conv_below" not in report.congested_conveyors
    assert len(report.congested_conveyors) == 1


def test_analyzer_multi_input_output_edges() -> None:
    """Validate machine bottlenecks with multiple inputs and outputs."""
    analyzer = FactoryStateAnalyzerTool()
    state = FactoryState(
        machines=[
            # Multiple inputs, only one is empty -> shortage
            MachineState(
                id="assembler_multi",
                status="operating",
                inputs=[
                    InventoryItem(item_id="iron_plate", amount=10.0),
                    InventoryItem(item_id="copper_wire", amount=0.0),
                ],
                outputs=[
                    InventoryItem(item_id="rotor", amount=5.0),
                ],
            ),
            # Multiple outputs, only one is full -> blocked
            MachineState(
                id="manufacturer_multi",
                status="operating",
                inputs=[
                    InventoryItem(item_id="rotor", amount=10.0),
                ],
                outputs=[
                    InventoryItem(
                        item_id="smart_plating", amount=100.0, max_amount=100.0
                    ),
                    InventoryItem(item_id="excess_waste", amount=0.0, max_amount=100.0),
                ],
            ),
        ]
    )
    report = analyzer.analyze(state)
    assert "assembler_multi" in report.input_shortages
    assert "manufacturer_multi" in report.output_blocked
    assert len(report.input_shortages) == 1
    assert len(report.output_blocked) == 1


def test_analyzer_empty_state_handling() -> None:
    """Validate empty or null state defaults correctly without errors."""
    analyzer = FactoryStateAnalyzerTool()

    # Case 1: Empty FactoryState
    state_empty = FactoryState()
    report1 = analyzer.analyze(state_empty)
    assert report1.average_operating_rate == 0.0
    assert len(report1.input_shortages) == 0
    assert len(report1.output_blocked) == 0
    assert len(report1.congested_conveyors) == 0
    assert report1.power_summary.power_issue is False

    # Case 2: None state
    report2 = analyzer.analyze(None)
    assert report2.average_operating_rate == 0.0
    assert len(report2.input_shortages) == 0
    assert len(report2.output_blocked) == 0
    assert len(report2.congested_conveyors) == 0
    assert report2.power_summary.power_issue is False
