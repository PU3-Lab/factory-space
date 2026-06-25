"""Unit tests for Process Optimizer LangGraph v2 Sprint 3."""

from agents.process_optimizer.graph import compile_process_optimizer_graph
from agents.process_optimizer.preview_store import preview_plan_store


def test_graph_import_and_compile():
    """그래프 모듈이 정상적으로 import되고 compile되는지 검증합니다."""

    graph = compile_process_optimizer_graph()
    assert graph is not None


def test_empty_factory_state_returns_error():
    """공장 상태가 없거나 비어 있으면 preview error payload를 반환합니다."""

    graph = compile_process_optimizer_graph()

    missing_state = {
        "session_id": "test-session-empty",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 12,
        },
    }

    result = graph.invoke(missing_state)
    assert result.get("error") is not None
    assert "Factory state is missing" in result["error"]

    preview = result.get("previewPayload")
    assert preview is not None
    assert preview["status"] == "error"
    assert "missing" in preview["summary"]

    empty_state = {
        "session_id": "test-session-empty",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 12,
            "factory_state": {
                "machines": [],
                "conveyors": [],
                "power_grid": {"produced": 100.0, "consumed": 50.0},
            },
        },
    }

    result_empty = graph.invoke(empty_state)
    assert result_empty.get("error") is not None
    assert "contains no machines or conveyors" in result_empty["error"]


def test_input_shortage_generates_preview_and_stores_top_level_session_id():
    """입력 부족 preview를 만들고 top-level session_id로 store에 저장합니다."""

    graph = compile_process_optimizer_graph()
    preview_plan_store.clear()

    session_id = "session-input-shortage"
    initial_state = {
        "session_id": session_id,
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 15,
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_01",
                        "type": "smelter",
                        "status": "operating",
                        "operating_rate": 0.5,
                        "inputs": [
                            {"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}
                        ],
                        "outputs": [
                            {"item_id": "iron_ingot", "amount": 5.0, "max_amount": 100.0}
                        ],
                        "power_consumption": 10.0,
                    }
                ],
                "conveyors": [],
                "power_grid": {"produced": 50.0, "consumed": 10.0},
            },
        },
    }

    result = graph.invoke(initial_state)
    assert result.get("error") is None

    preview = result.get("previewPayload")
    assert preview is not None
    assert preview["status"] == "preview"
    assert preview["factoryRevision"] == 15
    assert preview["goal"] == "balance"
    assert preview["expires_at"] is not None

    plan_id = preview["plan_id"]
    assert plan_id.startswith("plan-")

    saved_plan = preview_plan_store.get(session_id, plan_id)
    assert saved_plan is not None
    assert saved_plan.plan_id == plan_id
    assert saved_plan.session_id == session_id
    assert saved_plan.factoryRevision == 15
    assert len(saved_plan.changes) == 1

    changes = preview["changes"]
    assert len(changes) == 1
    assert changes[0]["id"] == "suggest_input_smelter_01"

    effect = preview["expected_effect"]
    assert effect["estimated"] is False
    assert "operating_rate_improvement" not in effect
    assert effect["resolved_input_shortages_count"] == 1
    assert effect["resolved_output_blocks_count"] == 0
    assert effect["resolved_conveyor_congestions_count"] == 0


def test_payload_session_id_is_supported_for_test_console_compatibility():
    """테스트 콘솔처럼 payload 안에 session_id가 들어오는 형태도 보조로 지원합니다."""

    graph = compile_process_optimizer_graph()
    preview_plan_store.clear()

    session_id = "payload-session"
    initial_state = {
        "payload": {
            "session_id": session_id,
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 16,
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_payload",
                        "status": "operating",
                        "inputs": [{"item_id": "ore", "amount": 0.0}],
                    }
                ],
                "conveyors": [],
                "power_grid": {"produced": 50.0, "consumed": 10.0},
            },
        },
    }

    result = graph.invoke(initial_state)
    preview = result["previewPayload"]
    saved_plan = preview_plan_store.get(session_id, preview["plan_id"])

    assert result.get("error") is None
    assert saved_plan is not None
    assert saved_plan.session_id == session_id


def test_output_blocked_generates_preview():
    """출력 적체 상태에서 출력 적체 preview 제안과 저장을 검증합니다."""

    graph = compile_process_optimizer_graph()
    preview_plan_store.clear()

    session_id = "session-output-blocked"
    initial_state = {
        "session_id": session_id,
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 17,
            "factory_state": {
                "machines": [
                    {
                        "id": "constructor_01",
                        "type": "constructor",
                        "status": "operating",
                        "operating_rate": 0.4,
                        "inputs": [
                            {
                                "item_id": "iron_ingot",
                                "amount": 20.0,
                                "max_amount": 100.0,
                            }
                        ],
                        "outputs": [
                            {
                                "item_id": "iron_plate",
                                "amount": 100.0,
                                "max_amount": 100.0,
                            }
                        ],
                        "power_consumption": 8.0,
                    }
                ],
                "conveyors": [],
                "power_grid": {"produced": 50.0, "consumed": 8.0},
            },
        },
    }

    result = graph.invoke(initial_state)
    assert result.get("error") is None

    preview = result.get("previewPayload")
    assert preview is not None

    saved_plan = preview_plan_store.get(session_id, preview["plan_id"])
    assert saved_plan is not None

    changes = preview["changes"]
    assert len(changes) == 1
    assert changes[0]["id"] == "suggest_output_constructor_01"

    effect = preview["expected_effect"]
    assert effect["estimated"] is False
    assert "operating_rate_improvement" not in effect
    assert effect["resolved_input_shortages_count"] == 0
    assert effect["resolved_output_blocks_count"] == 1


def test_conveyor_congestion_includes_ui_hints():
    """혼잡한 컨베이어만 ui_hints 하이라이트 대상에 포함합니다."""

    graph = compile_process_optimizer_graph()
    preview_plan_store.clear()

    initial_state = {
        "session_id": "session-conveyor",
        "payload": {
            "operation": "analyze",
            "goal": "congestion_relief",
            "factoryRevision": 18,
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_01",
                        "type": "smelter",
                        "status": "operating",
                        "operating_rate": 0.9,
                        "inputs": [{"item_id": "iron_ore", "amount": 50.0}],
                        "outputs": [{"item_id": "iron_ingot", "amount": 10.0}],
                    }
                ],
                "conveyors": [
                    {"id": "conv_bottleneck", "congestion_rate": 0.85},
                    {"id": "conv_smooth", "congestion_rate": 0.2},
                ],
                "power_grid": {"produced": 50.0, "consumed": 10.0},
            },
        },
    }

    result = graph.invoke(initial_state)
    assert result.get("error") is None

    preview = result.get("previewPayload")
    assert preview is not None

    ui_hints = preview["ui_hints"]
    assert "conv_bottleneck" in ui_hints["highlight_targets"]
    assert "conv_smooth" not in ui_hints["highlight_targets"]


def test_suggestions_limited_to_three():
    """여러 병목이 동시에 있어도 preview 변경 항목을 최대 3개로 제한합니다."""

    graph = compile_process_optimizer_graph()
    preview_plan_store.clear()

    initial_state = {
        "session_id": "session-limit-3",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 19,
            "factory_state": {
                "machines": [
                    {
                        "id": "mach_shortage_1",
                        "status": "operating",
                        "inputs": [{"item_id": "ore", "amount": 0.0}],
                    },
                    {
                        "id": "mach_shortage_2",
                        "status": "operating",
                        "inputs": [{"item_id": "ore", "amount": 0.0}],
                    },
                    {
                        "id": "mach_blocked_1",
                        "status": "operating",
                        "outputs": [
                            {"item_id": "plate", "amount": 100.0, "max_amount": 100.0}
                        ],
                    },
                    {
                        "id": "mach_blocked_2",
                        "status": "operating",
                        "outputs": [
                            {"item_id": "plate", "amount": 100.0, "max_amount": 100.0}
                        ],
                    },
                ],
                "conveyors": [],
                "power_grid": {"produced": 10.0, "consumed": 50.0},
            },
        },
    }

    result = graph.invoke(initial_state)
    assert result.get("error") is None

    preview = result.get("previewPayload")
    assert preview is not None

    changes = preview["changes"]
    assert len(changes) == 3
    assert preview["expected_effect"]["estimated"] is False
    assert "operating_rate_improvement" not in preview["expected_effect"]
