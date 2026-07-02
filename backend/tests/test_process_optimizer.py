"""Tests for process optimizer agent."""

from __future__ import annotations

import json
from pathlib import Path

import pytest
from pydantic import ValidationError

from agents.base import AgentContext
from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.agent import ProcessOptimizerAgent
from agents.process_optimizer.schemas import (
    FactoryState,
    GeneratorPowerState,
    ProcessOptimizerPayload,
    TargetDescriptor,
)
from agents.process_optimizer.session_memory import process_optimizer_memory

SAMPLE_DIR = Path(__file__).resolve().parents[2] / "docs" / "process_optimizer"


class StubLLM:
    """Mock LLM responses in sequential order."""

    def __init__(self, responses: list[str | None]) -> None:
        self.responses = responses
        self.calls = 0

    def invoke(self, prompt: str) -> str | None:
        if self.calls >= len(self.responses):
            return None
        res = self.responses[self.calls]
        self.calls += 1
        return res


def test_process_optimizer_payload_schema() -> None:
    """Validate ProcessOptimizerPayload schema parsing."""
    # Valid payloads
    p1 = ProcessOptimizerPayload.model_validate({"operation": "state_update"})
    assert p1.operation == "state_update"
    assert p1.goal == "balance"

    p2 = ProcessOptimizerPayload.model_validate(
        {"operation": "analyze", "goal": "throughput"}
    )
    assert p2.operation == "analyze"
    assert p2.goal == "throughput"

    p_apply = ProcessOptimizerPayload.model_validate({"operation": "apply"})
    assert p_apply.operation == "apply"

    p3 = ProcessOptimizerPayload.model_validate({})
    assert p3.operation == "analyze"
    assert p3.goal == "balance"

    # Invalid payloads
    with pytest.raises(ValidationError):
        ProcessOptimizerPayload.model_validate({"operation": "invalid_operation"})


def test_process_optimizer_memory() -> None:
    """Ensure that the session memory store works correctly."""
    session_id = "test-session-123"
    process_optimizer_memory.clear(session_id)

    assert process_optimizer_memory.get_state(session_id) == {}
    assert process_optimizer_memory.get_revision(session_id) == 0

    state = {"machines": [{"id": "smelter_1"}]}
    process_optimizer_memory.update(session_id, state, 42)

    assert process_optimizer_memory.get_state(session_id) == state
    assert process_optimizer_memory.get_revision(session_id) == 42

    process_optimizer_memory.clear(session_id)
    assert process_optimizer_memory.get_state(session_id) == {}


def test_process_optimizer_state_update_pipeline() -> None:
    """Ensure state_update requests successfully update session memory without LLM."""
    pipeline = AgentPipeline()
    session_id = "session-state-update"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-state-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "power_saving",
            "factoryRevision": 100,
            "factory_state": {"machines": [{"id": "m1"}, {"id": "m2"}]},
        },
    }

    res = pipeline.run(request_msg)

    # Check response payload
    assert res.get("type") == "agent.response", f"Pipeline error details: {res}"
    assert res["agent"] == "process_optimizer"
    assert res["payload"]["status"] == "success"
    assert res["payload"]["factoryRevision"] == 100
    assert res["payload"]["goal"] == "power_saving"

    # Check session memory has been populated
    assert process_optimizer_memory.get_revision(session_id) == 100
    assert process_optimizer_memory.get_state(session_id) == {
        "machines": [{"id": "m1"}, {"id": "m2"}]
    }


def test_process_optimizer_state_update_accepts_empty_factory_state() -> None:
    """Ensure an empty factory_state snapshot is treated as present and valid."""
    pipeline = AgentPipeline()
    session_id = "session-empty-state-update"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-state-empty",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "factoryRevision": 1,
            "factory_state": {},
        },
    }

    res = pipeline.run(request_msg)

    assert res.get("type") == "agent.response", f"Pipeline error details: {res}"
    assert res["payload"]["status"] == "success"
    assert res["payload"]["factoryRevision"] == 1
    assert process_optimizer_memory.get_revision(session_id) == 1
    assert process_optimizer_memory.get_state(session_id) == {}


def test_process_optimizer_analyze_uses_llm_for_player_explanation() -> None:
    """분석/명령 검증은 코드가 하고, 플레이어용 설명만 LLM으로 보강한다."""
    llm_response = json.dumps(
        {
            "summary": "제련기 입력 부족을 먼저 풀어 생산 흐름을 회복하는 계획입니다.",
            "player_message": "변경 대상을 확인한 뒤 적용할 항목을 승인하세요.",
            "change_explanations": [
                {
                    "id": "suggest_input_smelter_1",
                    "reason": "smelter_1은 원자재가 없어 낮은 가동률로 병목이 됩니다.",
                    "priority_explanation": "입력 부족을 해소해야 뒤 공정도 함께 살아납니다.",
                    "expected_effect_text": "철광석 공급이 회복되면 제련기 가동률이 개선됩니다.",
                }
            ],
        },
        ensure_ascii=False,
    )
    """Mock LLM responses in sequential order."""

    def __init__(self, responses: list[str | None]) -> None:
        self.responses = responses
        self.calls = 0

    def invoke(self, prompt: str) -> str | None:
        if self.calls >= len(self.responses):
            return None
        res = self.responses[self.calls]
        self.calls += 1
        return res


def test_process_optimizer_payload_schema() -> None:
    """Validate ProcessOptimizerPayload schema parsing."""
    # Valid payloads
    p1 = ProcessOptimizerPayload.model_validate({"operation": "state_update"})
    assert p1.operation == "state_update"
    assert p1.goal == "balance"

    p2 = ProcessOptimizerPayload.model_validate(
        {"operation": "analyze", "goal": "throughput"}
    )
    assert p2.operation == "analyze"
    assert p2.goal == "throughput"

    p_apply = ProcessOptimizerPayload.model_validate({"operation": "apply"})
    assert p_apply.operation == "apply"

    p3 = ProcessOptimizerPayload.model_validate({})
    assert p3.operation == "analyze"
    assert p3.goal == "balance"

    # Invalid payloads
    with pytest.raises(ValidationError):
        ProcessOptimizerPayload.model_validate({"operation": "invalid_operation"})


def test_process_optimizer_memory() -> None:
    """Ensure that the session memory store works correctly."""
    session_id = "test-session-123"
    process_optimizer_memory.clear(session_id)

    assert process_optimizer_memory.get_state(session_id) == {}
    assert process_optimizer_memory.get_revision(session_id) == 0

    state = {"machines": [{"id": "smelter_1"}]}
    process_optimizer_memory.update(session_id, state, 42)

    assert process_optimizer_memory.get_state(session_id) == state
    assert process_optimizer_memory.get_revision(session_id) == 42

    process_optimizer_memory.clear(session_id)
    assert process_optimizer_memory.get_state(session_id) == {}


def test_process_optimizer_state_update_pipeline() -> None:
    """Ensure state_update requests successfully update session memory without LLM."""
    pipeline = AgentPipeline()
    session_id = "session-state-update"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-state-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "power_saving",
            "factoryRevision": 100,
            "factory_state": {"machines": [{"id": "m1"}, {"id": "m2"}]},
        },
    }

    res = pipeline.run(request_msg)

    # Check response payload
    assert res.get("type") == "agent.response", f"Pipeline error details: {res}"
    assert res["agent"] == "process_optimizer"
    assert res["payload"]["status"] == "success"
    assert res["payload"]["factoryRevision"] == 100
    assert res["payload"]["goal"] == "power_saving"

    # Check session memory has been populated
    assert process_optimizer_memory.get_revision(session_id) == 100
    assert process_optimizer_memory.get_state(session_id) == {
        "machines": [{"id": "m1"}, {"id": "m2"}]
    }


def test_process_optimizer_state_update_accepts_empty_factory_state() -> None:
    """Ensure an empty factory_state snapshot is treated as present and valid."""
    pipeline = AgentPipeline()
    session_id = "session-empty-state-update"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-state-empty",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "factoryRevision": 1,
            "factory_state": {},
        },
    }

    res = pipeline.run(request_msg)

    assert res.get("type") == "agent.response", f"Pipeline error details: {res}"
    assert res["payload"]["status"] == "success"
    assert res["payload"]["factoryRevision"] == 1
    assert process_optimizer_memory.get_revision(session_id) == 1
    assert process_optimizer_memory.get_state(session_id) == {}


def test_process_optimizer_analyze_uses_llm_for_player_explanation() -> None:
    """분석/명령 검증은 코드가 하고, 플레이어용 설명만 LLM으로 보강한다."""
    llm_response = json.dumps(
        {
            "summary": "제련기 입력 부족을 먼저 풀어 생산 흐름을 회복하는 계획입니다.",
            "player_message": "변경 대상을 확인한 뒤 적용할 항목을 승인하세요.",
            "change_explanations": [
                {
                    "id": "inspect_iron_ore_supply_smelter_1",
                    "reason": "smelter_1은 원자재가 없어 낮은 가동률로 병목이 됩니다.",
                    "priority_explanation": "입력 부족을 해소해야 뒤 공정도 함께 살아납니다.",
                    "expected_effect_text": "철광석 공급이 회복되면 제련기 가동률이 개선됩니다.",
                }
            ],
        },
        ensure_ascii=False,
    )
    pipeline = AgentPipeline(llm=StubLLM([llm_response]))

    res = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-process-llm-explain",
            "session_id": "session-process-llm-explain",
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "analyze",
                "goal": "balance",
                "factoryRevision": 1,
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_1",
                            "type": "smelter",
                            "status": "operating",
                            "operating_rate": 0.2,
                            "inputs": [
                                {
                                    "item_id": "iron_ore",
                                    "amount": 0.0,
                                    "max_amount": 100.0,
                                }
                            ],
                        }
                    ],
                    "conveyors": [],
                    "power_grid": {"produced": 100.0, "consumed": 50.0},
                    "storages": [
                        {
                            "id": "storage_1",
                            "type": "storage",
                            "inventory": [{"item_id": "iron_ore", "amount": 100.0}]
                        }
                    ],
                    "resource_nodes": [{"id": "node_1"}]
                },
            },
        }
    )

    assert res["type"] == "agent.response"
    assert res["payload"]["status"] == "preview"
    assert res["payload"]["summary"] == "제련기 입력 부족을 먼저 풀어 생산 흐름을 회복하는 계획입니다."
    assert res["payload"]["player_message"] == "변경 대상을 확인한 뒤 적용할 항목을 승인하세요."
    assert res["payload"]["changes"][0]["reason"] == "smelter_1은 원자재가 없어 낮은 가동률로 병목이 됩니다."
    assert res["payload"]["metadata"]["llm"] == "used"


def test_process_optimizer_analyze_fallback_or_mock() -> None:
    """Ensure analyze requests use the v2 preview graph and session memory."""
    pipeline = AgentPipeline(llm=StubLLM(["process_optimizer", None]))
    session_id = "session-analyze-test"
    process_optimizer_memory.clear(session_id)

    # Put state in memory beforehand
    state = {
        "machines": [
            {
                "id": "m1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 50.0, "consumed": 10.0},
        "storages": [
            {
                "id": "storage_1",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 100.0}]
            }
        ],
        "resource_nodes": [{"id": "node_1"}]
    }
    process_optimizer_memory.update(session_id, state, 10)

    # Run analyze request (without LLM mocking, should trigger fallback when LLM fails or fallback node runs)
    request_msg = {
        "type": "agent.request",
        "request_id": "req-analyze-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {"operation": "analyze", "goal": "balance"},
    }

    # Execute run
    res = pipeline.run(request_msg)

    # Should fall back to process_optimizer fallback structure
    assert res.get("type") == "agent.response", f"Pipeline error details: {res}"
    assert res["agent"] == "process_optimizer"
    assert res["payload"]["status"] == "preview"
    assert res["payload"]["factoryRevision"] == 10
    assert "summary" in res["payload"]
    assert "changes" in res["payload"]
    assert "suggestions" in res["payload"]


def test_process_optimizer_analyze_defaults_when_operation_omitted() -> None:
    """operation???앸왂?섎㈃ ProcessOptimizerPayload 湲곕낯媛믪쿂??analyze ?붿껌?쇰줈 泥섎━?⑸땲??"""
    pipeline = AgentPipeline(llm=StubLLM(["process_optimizer", None]))
    session_id = "session-analyze-default-operation"
    process_optimizer_memory.clear(session_id)
    process_optimizer_memory.update(
        session_id,
        {
            "machines": [
                {
                    "id": "m1",
                    "type": "smelter",
                    "status": "operating",
                    "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                }
            ],
            "conveyors": [],
            "power_grid": {"produced": 50.0, "consumed": 10.0},
            "storages": [
                {
                    "id": "storage_1",
                    "type": "storage",
                    "inventory": [{"item_id": "iron_ore", "amount": 100.0}]
                }
            ],
            "resource_nodes": [{"id": "node_1"}]
        },
        10,
    )

    request_msg = {
        "type": "agent.request",
        "request_id": "req-analyze-default-operation",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {"goal": "balance"},
    }

    res = pipeline.run(request_msg)

    assert res.get("type") == "agent.response", f"Pipeline error details: {res}"
    assert res["agent"] == "process_optimizer"
    assert res["payload"]["status"] == "preview"
    assert res["payload"]["goal"] == "balance"


def test_process_optimizer_analyze_uses_payload_revision_and_state() -> None:
    """Sprint 5 Unreal contract sends factoryRevision and factory_state in payload."""
    pipeline = AgentPipeline(llm=StubLLM(["process_optimizer", None]))
    session_id = "session-analyze-payload-contract"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-analyze-payload-contract",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "congestion_relief",
            "factoryRevision": 12,
            "factory_state": {
                "machines": [
                    {
                        "id": "constructor_1",
                        "type": "constructor",
                        "status": "operating",
                        "operating_rate": 0.55,
                        "outputs": [
                            {
                                "item_id": "iron_plate",
                                "amount": 100.0,
                                "max_amount": 100.0,
                            }
                        ],
                    }
                ],
                "conveyors": [
                    {
                        "id": "conv_output_constructor_1",
                        "congestion_rate": 0.95,
                    }
                ],
                "power_grid": {"produced": 120.0, "consumed": 90.0},
            },
        },
        "context": {"language": "ko", "mode": "gameplay"},
    }

    res = pipeline.run(request_msg)

    assert res.get("type") == "agent.response", f"Pipeline error details: {res}"
    assert res["agent"] == "process_optimizer"
    assert res["payload"]["status"] == "preview"
    assert res["payload"]["factoryRevision"] == 12
    assert res["payload"]["goal"] == "congestion_relief"
    assert "constructor_1" in res["payload"]["ui_hints"]["highlight_targets"]
    assert (
        "conv_output_constructor_1" in res["payload"]["ui_hints"]["highlight_targets"]
    )


def test_agent_direct_methods() -> None:
    """Test ProcessOptimizerAgent's build_prompt and fallback directly."""
    agent = ProcessOptimizerAgent()
    context = AgentContext(request_id="req-direct", session_id="session-direct")

    process_optimizer_memory.clear("session-direct")
    process_optimizer_memory.update("session-direct", {"machines": [{"id": "m1"}]}, 15)

    prompt = agent.build_prompt({"goal": "throughput"}, context)
    assert "m1" in prompt
    assert "throughput" in prompt
    assert "15" in prompt

    result = agent.fallback({}, context)
    assert result.agent == "process_optimizer"
    assert "1개 설비" in result.payload["summary"]


def test_unreal_websocket_contract_sample_validation() -> None:
    """Validate every Unreal sample request against the current payload schemas."""

    sample_path = SAMPLE_DIR / "agent_test_sample.json"
    assert sample_path.exists()

    with open(sample_path, encoding="utf-8") as f:
        data = json.load(f)

    expected_samples = {
        "analyze_request": "analyze",
        "apply_request": "apply",
        "undo_request": "undo",
        "measure_request": "measure",
    }
    assert set(expected_samples).issubset(data.keys())

    for sample_key, expected_operation in expected_samples.items():
        envelope = data[sample_key]
        assert envelope["type"] == "agent.request"
        assert envelope["agent"] == "process_optimizer"
        payload_obj = ProcessOptimizerPayload.model_validate(envelope["payload"])
        assert payload_obj.operation == expected_operation

    payload_data = data["analyze_request"]["payload"]
    assert payload_data["goal"] == "congestion_relief"
    assert payload_data["factoryRevision"] == 12

    state_data = payload_data["factory_state"]
    state_obj = FactoryState.model_validate(state_data)

    assert len(state_obj.machines) == 2
    assert state_obj.machines[0].id == "constructor_1"
    assert state_obj.machines[0].operating_rate == 0.55
    assert state_obj.machines[0].outputs[0].amount == 100.0
    assert state_obj.machines[1].status == "disabled"

    assert len(state_obj.conveyors) == 1
    assert state_obj.conveyors[0].id == "conv_output_constructor_1"
    assert state_obj.conveyors[0].congestion_rate == 0.95

    assert state_obj.power_grid.produced == 120.0
    assert state_obj.power_grid.consumed == 90.0


def test_process_optimizer_invalid_payload_validation() -> None:
    """Validate that invalid factory_state or factoryRevision raises validation error."""
    pipeline = AgentPipeline()
    session_id = "session-invalid-validation"

    # Case 1: factory_state has invalid field type (machines is a string instead of a list)
    request_msg = {
        "type": "agent.request",
        "request_id": "req-invalid-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "factoryRevision": 10,
            "factory_state": {"machines": "not-a-list"},
        },
    }
    res = pipeline.run(request_msg)
    assert res.get("type") == "agent.error"
    assert "INVALID_REQUEST_PAYLOAD" in res["error"]["code"]

    # Case 2: factoryRevision is negative
    request_msg_negative_rev = {
        "type": "agent.request",
        "request_id": "req-invalid-2",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "factoryRevision": -5,
            "factory_state": {"machines": []},
        },
    }
    res = pipeline.run(request_msg_negative_rev)
    assert res.get("type") == "agent.error"
    assert "INVALID_REQUEST_PAYLOAD" in res["error"]["code"]


def test_unreal_websocket_contract_power_grid_sample_validation() -> None:
    """Validate the new power grid sample snapshot JSON against schemas."""

    sample_path = SAMPLE_DIR / "agent_test_power_grid_sample.json"
    assert sample_path.exists()

    with open(sample_path, encoding="utf-8") as f:
        data = json.load(f)

    expected_samples = {
        "state_update_request": "state_update",
        "analyze_request": "analyze",
    }
    assert set(expected_samples).issubset(data.keys())

    for sample_key, expected_operation in expected_samples.items():
        envelope = data[sample_key]
        assert envelope["type"] == "agent.request"
        assert envelope["agent"] == "process_optimizer"
        payload_obj = ProcessOptimizerPayload.model_validate(envelope["payload"])
        assert payload_obj.operation == expected_operation

    state_data = data["state_update_request"]["payload"]["factory_state"]
    state_obj = FactoryState.model_validate(state_data)

    assert len(state_obj.machines) == 1
    assert state_obj.machines[0].id == "smelter_1"
    assert state_obj.machines[0].connected_power_node_ids == ["pole_30"]

    assert len(state_obj.power_grid.nodes) == 2
    assert state_obj.power_grid.nodes[0].id == "pole_1"
    assert state_obj.power_grid.nodes[0].connected_node_ids == ["pole_2"]
    assert state_obj.power_grid.nodes[0].connected_machine_ids == ["generator_1"]

    assert len(state_obj.power_grid.generators) == 2
    assert state_obj.power_grid.generators[0].id == "generator_1"
    assert state_obj.power_grid.generators[0].connected is True
    assert state_obj.power_grid.generators[0].connected_power_node_ids == ["pole_1"]


def test_power_grid_schema_specifics() -> None:
    """기획서의 예상 테스트 요구사항들을 검증합니다."""
    # 1. nodes와 generators가 없는 기존 factory_state도 통과한다.
    legacy_state = {
        "machines": [{"id": "m1", "type": "smelter"}],
        "conveyors": [],
        "power_grid": {
            "produced": 100.0,
            "consumed": 50.0
        }
    }
    state_obj = FactoryState.model_validate(legacy_state)
    assert len(state_obj.power_grid.nodes) == 0
    assert len(state_obj.power_grid.generators) == 0

    # 2. nodes와 generators가 있는 전력망 factory_state도 통과한다.
    power_state = {
        "machines": [{"id": "m1", "connected_power_node_ids": ["pole_1"]}],
        "power_grid": {
            "produced": 200.0,
            "consumed": 150.0,
            "nodes": [
                {"id": "pole_1", "type": "power_pole", "connected_node_ids": ["pole_2"]}
            ],
            "generators": [
                {"id": "gen_1", "produced": 100.0, "connected": True, "connected_power_node_ids": ["pole_2"]}
            ]
        }
    }
    state_obj2 = FactoryState.model_validate(power_state)
    assert len(state_obj2.power_grid.nodes) == 1
    assert state_obj2.power_grid.nodes[0].type == "power_pole"
    assert len(state_obj2.power_grid.generators) == 1

    # 3. connected=true이지만 connected_power_node_ids=[]인 발전기는 schema에서 받을 수 있다.
    gen_state = {
        "id": "generator_5",
        "produced": 30.0,
        "connected": True,
        "connected_power_node_ids": []
    }
    gen_obj = GeneratorPowerState.model_validate(gen_state)
    assert gen_obj.connected is True
    assert gen_obj.connected_power_node_ids == []

    assert gen_obj.connected is True
    assert gen_obj.connected_power_node_ids == []

    # 4. TargetDescriptor(type="power_pole", id="pole_30")가 통과한다.
    target_pole = TargetDescriptor.model_validate({"type": "power_pole", "id": "pole_30"})
    assert target_pole.type == "power_pole"
    assert target_pole.id == "pole_30"

    # 5. TargetDescriptor(type="generator", id="generator_5")가 통과한다.
    target_gen = TargetDescriptor.model_validate({"type": "generator", "id": "generator_5"})
    assert target_gen.type == "generator"
    assert target_gen.id == "generator_5"


def test_power_grid_graph_analysis_logic() -> None:
    """Validate Sprint 2 graph traversal and connectivity analysis logic."""
    from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
    from agents.process_optimizer.schemas import FactoryState

    analyzer = FactoryStateAnalyzerTool()

    # 1. nodes가 비어 있으면 기존 produced/consumed 분석만 수행한다.
    empty_nodes_state = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "idle",
                "power_consumption": 15.0,
                "connected_power_node_ids": ["pole_30"]
            }
        ],
        "conveyors": [],
        "power_grid": {
            "produced": 120.0,
            "consumed": 150.0,
            "nodes": [],
            "generators": []
        }
    }
    report = analyzer.analyze(empty_nodes_state)
    assert report.power_summary.power_issue is True
    assert report.isolated_power_nodes == []
    assert report.disconnected_generators == []
    assert report.unpowered_machines == []

    # 2. 기획서의 분석 및 예상 테스트 케이스 종합 검증
    # - pole_30이 연결되지 않은 케이스에서 isolated_power_nodes에 포함된다.
    # - generator_5의 connected_power_node_ids가 빈 배열이면 disconnected_generators에 포함된다.
    # - smelter_1이 발전기 없는 component(pole_30)에 연결되어 있으면 unpowered_machines에 포함된다.
    # - 여러 발전기 component가 있을 때 각각 powered component로 계산된다.
    complex_state = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "idle",
                "power_consumption": 15.0,
                "connected_power_node_ids": ["pole_30"]
            },
            {
                "id": "constructor_1",
                "type": "constructor",
                "status": "operating",
                "power_consumption": 4.0,
                "connected_power_node_ids": ["pole_2"]
            },
            {
                "id": "generator_5",
                "type": "generator",
                "status": "operating",
                "power_consumption": 0.0,
                "connected_power_node_ids": []
            }
        ],
        "conveyors": [],
        "power_grid": {
            "produced": 120.0,
            "consumed": 150.0,
            "nodes": [
                {
                    "id": "pole_1",
                    "type": "power_pole",
                    "connected_node_ids": ["pole_2"],
                    "connected_machine_ids": ["generator_1"]
                },
                {
                    "id": "pole_2",
                    "type": "power_pole",
                    "connected_node_ids": ["pole_1"],
                    "connected_machine_ids": ["constructor_1"]
                },
                {
                    "id": "pole_30",
                    "type": "power_pole",
                    "connected_node_ids": [],
                    "connected_machine_ids": ["smelter_1"]
                }
            ],
            "generators": [
                {
                    "id": "generator_1",
                    "produced": 30.0,
                    "connected": True,
                    "connected_power_node_ids": ["pole_1"]
                },
                {
                    "id": "generator_5",
                    "produced": 30.0,
                    "connected": False,
                    "connected_power_node_ids": []
                }
            ]
        }
    }
    report2 = analyzer.analyze(complex_state)

    # 30개 송전탑 중 1개 고립 케이스 탐지
    assert "pole_30" in report2.isolated_power_nodes
    assert "pole_1" not in report2.isolated_power_nodes
    assert "pole_2" not in report2.isolated_power_nodes

    # 5개 발전기 중 1개 미연결 케이스 탐지
    assert "generator_5" in report2.disconnected_generators
    assert "generator_1" not in report2.disconnected_generators

    # 고립 송전탑에 연결된 smelter_1은 unpowered, 유효 컴포넌트에 연결된 constructor_1은 powered
    assert "smelter_1" in report2.unpowered_machines
    assert "constructor_1" not in report2.unpowered_machines
    assert "generator_5" not in report2.unpowered_machines


def test_power_grid_suggestion_and_highlight() -> None:
    """Validate Sprint 3 suggestion generation, highlight target aggregation, and prompt updates."""
    from agents.process_optimizer.schemas import (
        FactoryAnalysisReport,
        OptimizationSuggestion,
        PowerSummary,
    )
    from agents.process_optimizer.suggestion import (
        OptimizationSuggestionTool,
        SuggestionValidationTool,
    )

    suggestion_tool = OptimizationSuggestionTool()

    # 1. isolated_power_nodes, disconnected_generators, unpowered_machines 존재 시 제안 및 하이라이트 검증
    report = FactoryAnalysisReport(
        factoryRevision=15,
        goal="balance",
        average_operating_rate=0.5,
        power_summary=PowerSummary(produced=100.0, consumed=120.0, power_issue=True),
        isolated_power_nodes=["pole_30"],
        disconnected_generators=["generator_5"],
        unpowered_machines=["smelter_1"]
    )

    suggestions, ui_hints = suggestion_tool.generate_suggestions(report)

    # 제안 생성 검증
    isolated_suggestion = next((s for s in suggestions if s.id == "inspect_power_pole_30"), None)
    assert isolated_suggestion is not None
    assert isolated_suggestion.target.type == "power_pole"
    assert isolated_suggestion.target.id == "pole_30"
    assert "pole_30이 주 전력망과 연결되어 있지 않습니다." in isolated_suggestion.problem
    assert "smelter_1" in isolated_suggestion.expected_effect  # expected_effect에 전력 미공급 설비 포함 여부 검증
    assert isolated_suggestion.risk == "medium"

    gen_suggestion = next((s for s in suggestions if s.id == "inspect_generator_5_power_connection"), None)
    assert gen_suggestion is not None
    assert gen_suggestion.target.type == "generator"
    assert gen_suggestion.target.id == "generator_5"
    assert "generator_5가 전력망에 연결되어 있지 않습니다." in gen_suggestion.problem
    assert gen_suggestion.risk == "medium"

    # 하이라이트 타겟 검증
    assert "pole_30" in ui_hints.highlight_targets
    assert "generator_5" in ui_hints.highlight_targets
    assert "smelter_1" in ui_hints.highlight_targets

    # 2. preview changes 최대 3개 개수 제한 유지 검증
    # 전력망 제안 2개 + 기존 input_shortages 2개 = 총 4개 후보 중 정렬하여 최대 3개만 반환하는지
    crowded_report = FactoryAnalysisReport(
        factoryRevision=15,
        goal="balance",
        average_operating_rate=0.5,
        input_shortages=["m1", "m2"],
        power_summary=PowerSummary(produced=100.0, consumed=120.0, power_issue=True),
        isolated_power_nodes=["pole_30"],
        disconnected_generators=["generator_5"],
        unpowered_machines=["smelter_1"]
    )
    sugs, hints = suggestion_tool.generate_suggestions(crowded_report)
    assert len(sugs) == 3

    # 3. LLM 프롬프트에 전력망 수동 연결 지침이 포함되는지 검증
    from agents.process_optimizer.prompts import build_process_optimizer_explanation_prompt
    mock_payload = {
        "status": "preview",
        "factoryRevision": 15,
        "goal": "balance",
        "changes": [s.model_dump() for s in sugs]
    }
    prompt = build_process_optimizer_explanation_prompt(mock_payload)
    assert "manual cable connection" in prompt

    # 4. 자동 전력 연결 명령 문구가 섞이면 검증에서 차단한다.
    validator = SuggestionValidationTool()
    unsafe_suggestion = OptimizationSuggestion(
        id="unsafe_power_command",
        target=TargetDescriptor(type="power_pole", id="pole_30"),
        problem="pole_30이 고립되어 있습니다.",
        recommended_action="connect_power_line 명령으로 자동 연결하십시오.",
        expected_effect="자동 전선 연결을 시도합니다.",
        risk="medium",
        confidence=1.0,
    )
    assert validator.validate_suggestions([unsafe_suggestion]) is False


def test_power_grid_subquest_flow() -> None:
    """Validate Sprint 4 subquest alerts and pipeline target-analyze flow."""
    from agents.process_optimizer.schemas import FactoryAnalysisReport, PowerSummary
    from agents.process_optimizer.subquest_alert import SubquestAlertBuilder
    from agents.pipeline.runtime import AgentPipeline

    alert_builder = SubquestAlertBuilder()

    # 1. 고립 송전탑 이슈 subquest 발행 검증
    report = FactoryAnalysisReport(
        factoryRevision=12,
        goal="balance",
        average_operating_rate=0.5,
        power_summary=PowerSummary(produced=100.0, consumed=120.0, power_issue=False),
        isolated_power_nodes=["pole_30"],
        disconnected_generators=[],
        unpowered_machines=["smelter_1"]
    )
    factory_state = {
        "machines": [{"id": "smelter_1", "connected_power_node_ids": ["pole_30"]}],
        "conveyors": [],
        "power_grid": {
            "produced": 100.0,
            "consumed": 120.0,
            "nodes": [{"id": "pole_30", "connected_node_ids": []}],
            "generators": []
        }
    }
    alert = alert_builder.build_alert(report, factory_state, subquest_mode=True)
    assert alert.needed is True
    assert alert.severity == "medium"
    assert alert.target.type == "power_pole"
    assert alert.target.id == "pole_30"
    assert alert.suggested_subquest is not None
    assert alert.suggested_subquest.title == "고립된 송전탑 확인"
    assert "smelter_1" in alert.suggested_subquest.objective
    assert alert.suggested_subquest.next_request.request_source == "subquest"
    assert alert.suggested_subquest.next_request.target.id == "pole_30"

    # 2. 미연결 발전기 이슈 subquest 발행 검증
    report_gen = FactoryAnalysisReport(
        factoryRevision=12,
        goal="balance",
        average_operating_rate=0.5,
        power_summary=PowerSummary(produced=100.0, consumed=120.0, power_issue=False),
        isolated_power_nodes=[],
        disconnected_generators=["generator_5"],
        unpowered_machines=[]
    )
    alert_gen = alert_builder.build_alert(report_gen, factory_state, subquest_mode=True)
    assert alert_gen.needed is True
    assert alert_gen.target.type == "generator"
    assert alert_gen.target.id == "generator_5"
    assert alert_gen.suggested_subquest.title == "미연결 발전기 확인"
    assert alert_gen.suggested_subquest.next_request.request_source == "subquest"

    # 3. state_update -> subquest -> analyze 파이프라인 흐름 검증
    pipeline = AgentPipeline()
    session_id = "session-power-subquest-flow"
    process_optimizer_memory.clear(session_id)

    # 3.1. state_update 전송
    update_res = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s4-update",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "subquest_mode": True,
            "goal": "power_saving",
            "factoryRevision": 40,
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "unpowered",
                        "connected_power_node_ids": ["pole_30"]
                    }
                ],
                "conveyors": [],
                "power_grid": {
                    "produced": 120.0,
                    "consumed": 90.0,
                    "nodes": [
                        {
                            "id": "pole_30",
                            "type": "power_pole",
                            "connected_node_ids": [],
                            "connected_machine_ids": ["smelter_1"]
                        }
                    ],
                    "generators": [
                        {
                            "id": "generator_5",
                            "produced": 30.0,
                            "connected": False,
                            "connected_power_node_ids": []
                        }
                    ]
                }
            }
        }
    })
    assert update_res["type"] == "agent.response"
    assert update_res["payload"]["status"] == "success"
    alert_payload = update_res["payload"]["optimization_alert"]
    assert alert_payload["needed"] is True
    assert alert_payload["suggested_subquest"]["title"] == "고립된 송전탑 확인"
    update_highlights = update_res["payload"]["ui_hints"]["highlight_targets"]
    assert "pole_30" in update_highlights
    assert "generator_5" in update_highlights
    assert "smelter_1" in update_highlights
    # 송전탑 고립이 발전기 미연결보다 높은 우선순위이므로 고립 송전탑 서브퀘스트가 발행되어야 함
    assert alert_payload["suggested_subquest"]["title"] == "고립된 송전탑 확인"

    # 3.2. 서브퀘스트 수락으로 가정한 analyze 요청 전송 (target 기반)
    analyze_res = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s4-subquest-analyze",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "power_saving",
            "request_source": "subquest",
            "target": {
                "type": "power_pole",
                "id": "pole_30"
            }
        }
    })
    assert analyze_res["type"] == "agent.response"
    assert analyze_res["payload"]["status"] == "preview"
    # target-analyze 흐름에서 target_id가 ui_hints.highlight_targets에 올바르게 포함되었는지 검사
    assert "pole_30" in analyze_res["payload"]["ui_hints"]["highlight_targets"]
    # commands에 불필요한 자동 조작이 없는지 확인
    assert not analyze_res["payload"].get("commands")


def test_power_grid_snapshot_store_workflow() -> None:
    """Validate Sprint 5 snapshot store functionality (save, upsert, analyze fallback, error handling)."""
    from agents.process_optimizer.snapshot_store import process_optimizer_snapshot_store
    from agents.pipeline.runtime import AgentPipeline

    session_id = "session-power-snapshot-workflow"
    client_id = "unreal-client-1"

    # 스토어 청소
    process_optimizer_snapshot_store.clear(session_id)

    factory_state_v42 = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 10.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 80.0},
        "storages": [
            {
                "id": "storage_1",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 100.0}]
            }
        ],
        "resource_nodes": [{"id": "node_1"}]
    }

    # 1. state_update 전송 시 snapshot_store에 정상 저장되는지 검증 (시나리오 1)
    pipeline = AgentPipeline()
    res1 = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s5-save",
        "session_id": session_id,
        "client_id": client_id,
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "balance",
            "factoryRevision": 42,
            "factory_state": factory_state_v42
        }
    })
    assert res1["type"] == "agent.response"
    assert res1["payload"]["status"] == "success"

    snapshot = process_optimizer_snapshot_store.get_latest(session_id, client_id)
    assert snapshot is not None
    assert snapshot.factoryRevision == 42
    assert snapshot.factory_state["machines"][0]["id"] == "smelter_1"
    assert snapshot.source == "state_update"

    # 2. 동일 세션에 새 revision 43이 유입될 때 갱신(Upsert)되는지 검증 (시나리오 2)
    factory_state_v43 = dict(factory_state_v42)
    factory_state_v43["machines"] = [
        {
            "id": "smelter_1",
            "type": "smelter",
            "status": "idle",
            "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
        }
    ]
    res2 = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s5-upsert",
        "session_id": session_id,
        "client_id": client_id,
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "balance",
            "factoryRevision": 43,
            "factory_state": factory_state_v43
        }
    })
    assert res2["type"] == "agent.response"

    snapshot_updated = process_optimizer_snapshot_store.get_latest(session_id, client_id)
    assert snapshot_updated.factoryRevision == 43
    assert snapshot_updated.factory_state["machines"][0]["status"] == "idle"

    # 2-1. 네트워크 지연 등으로 낮은 revision이 늦게 도착해도 최신 snapshot을 덮지 않아야 한다.
    res_old = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s5-stale-revision",
        "session_id": session_id,
        "client_id": client_id,
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "balance",
            "factoryRevision": 42,
            "factory_state": factory_state_v42
        }
    })
    assert res_old["type"] == "agent.response"

    snapshot_after_old = process_optimizer_snapshot_store.get_latest(session_id, client_id)
    assert snapshot_after_old.factoryRevision == 43
    assert snapshot_after_old.factory_state["machines"][0]["status"] == "idle"

    # 2-2. factory_state가 없는 빈 state_update도 기존 snapshot을 지우면 안 된다.
    res_empty = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s5-empty-state-update",
        "session_id": session_id,
        "client_id": client_id,
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "balance",
            "factoryRevision": 44
        }
    })
    assert res_empty["type"] == "agent.error" or res_empty["payload"]["status"] == "success"

    snapshot_after_empty = process_optimizer_snapshot_store.get_latest(session_id, client_id)
    assert snapshot_after_empty.factoryRevision == 43
    assert snapshot_after_empty.factory_state["machines"][0]["status"] == "idle"

    # 3. analyze 요청에 factory_state가 없고 저장된 snapshot이 있을 때 이를 가져와 정상 분석하는지 검증 (시나리오 3)
    res3 = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s5-analyze-using-snapshot",
        "session_id": session_id,
        "client_id": client_id,
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance"
            # factory_state 생략
        }
    })
    assert res3["type"] == "agent.response"
    assert res3["payload"]["status"] == "preview"
    assert res3["payload"]["factoryRevision"] == 43  # 저장된 리비전 43이 반영됨

    # 4. 저장 스냅샷이 없고 payload에도 없을 경우, 요청을 거절하는 안전 오류 검증 (시나리오 4)
    process_optimizer_snapshot_store.clear(session_id)
    # 기존 session_memory도 청소해두어야 온전히 snapshot_store와 fallback 미작동 에러를 검출할 수 있음
    from agents.process_optimizer.session_memory import process_optimizer_memory
    process_optimizer_memory.clear(session_id)

    res4 = pipeline.run({
        "type": "agent.request",
        "request_id": "req-s5-no-snapshot-error",
        "session_id": session_id,
        "client_id": client_id,
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance"
            # factory_state 생략, snapshot_store 비어있음
        }
    })
    # AgentPipeline 레벨에서 payload validation 실패 오류 응답 확인
    assert res4["type"] == "agent.error" or res4.get("payload", {}).get("status") == "error"


def test_power_grid_inventory_analysis_workflow() -> None:
    """Validate Sprint 6 inventory analysis (supply line issues vs production shortage, fallbacks, aggregate stocks)."""
    from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
    from agents.process_optimizer.suggestion import OptimizationSuggestionTool
    from agents.process_optimizer.subquest_alert import SubquestAlertBuilder

    analyzer = FactoryStateAnalyzerTool()
    suggestion_tool = OptimizationSuggestionTool()
    alert_builder = SubquestAlertBuilder()

    # 1. 창고에 철광석 재고가 충분한 경우 (시나리오 1 - 공급 라인 문제 제안)
    factory_state_with_stock = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0},
        "storages": [
            {
                "id": "storage_iron_ore_1",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 300.0, "max_amount": 500.0}],
            }
        ],
        "resource_nodes": [{"id": "node_1"}]
    }

    report1 = analyzer.analyze(factory_state_with_stock, goal="balance")
    # input_shortages 및 부족 아이템 매핑 검사
    assert "smelter_1" in report1.input_shortages
    assert report1.input_shortages_items.get("smelter_1") == "iron_ore"
    assert len(report1.storages) == 1
    assert report1.storages[0].id == "storage_iron_ore_1"

    suggestions1, hints1 = suggestion_tool.generate_suggestions(report1)
    sug_supply = next((s for s in suggestions1 if s.id == "inspect_iron_ore_supply_smelter_1"), None)
    # 공급 라인 문제 제안 확인
    assert sug_supply is not None
    assert sug_supply.target.id == "smelter_1"
    assert "supply" in sug_supply.id
    assert "컨베이어 연결을 확인하십시오" in sug_supply.recommended_action
    assert sug_supply.risk == "low"

    # 서브퀘스트 발행 확인 (공급 라인 문제)
    alert1 = alert_builder.build_alert(report1, factory_state_with_stock, subquest_mode=True)
    assert alert1.needed is True
    assert alert1.suggested_subquest.title == "철광석 공급 라인 점검"
    assert "컨베이어 연결을 확인하여 공급을 복구하세요" in alert1.suggested_subquest.objective

    # 2. 창고조차 비어있는 경우 (시나리오 2 - 생산/채굴 부족 제안)
    factory_state_empty_stock = dict(factory_state_with_stock)
    factory_state_empty_stock["storages"] = [
        {
            "id": "storage_iron_ore_1",
            "type": "storage",
            "inventory": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 500.0}],
        }
    ]

    report2 = analyzer.analyze(factory_state_empty_stock, goal="balance")
    suggestions2, hints2 = suggestion_tool.generate_suggestions(report2)
    sug_prod = next((s for s in suggestions2 if s.id == "expand_iron_ore_production_smelter_1"), None)
    # 생산 확충 제안 확인
    assert sug_prod is not None
    assert "production" in sug_prod.id
    assert "채굴기나 생산 시설을 확충" in sug_prod.recommended_action
    assert sug_prod.risk == "medium"

    # 서브퀘스트 발행 확인 (생산 확충)
    alert2 = alert_builder.build_alert(report2, factory_state_empty_stock, subquest_mode=True)
    assert alert2.needed is True
    assert alert2.suggested_subquest.title == "철광석 생산량 확충"
    assert "채굴기나 생산 시설을 확충하여 절대적인 공급량을 늘리세요" in alert2.suggested_subquest.objective

    # 3. storage 정보가 없는 경우 (시나리오 3 - Sprint 8에 따른 need_more_state 유도 검증)
    factory_state_no_storage = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0},
    }
    report3 = analyzer.analyze(factory_state_no_storage, goal="balance")
    assert report3.need_more_state is not None
    assert "storage_inventory" in report3.need_more_state["required_state_scopes"]

    suggestions3, hints3 = suggestion_tool.generate_suggestions(report3)
    assert not suggestions3

    alert3 = alert_builder.build_alert(report3, factory_state_no_storage, subquest_mode=True)
    assert alert3.needed is False

    # 4. 여러 창고에 재고가 분산 배치되어 있는 경우의 총량 합산 검증 (시나리오 4 - 30 + 50 = 80 > 0)
    factory_state_split_stock = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0},
        "storages": [
            {
                "id": "storage_iron_ore_1",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 30.0, "max_amount": 500.0}],
            },
            {
                "id": "storage_iron_ore_2",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 50.0, "max_amount": 500.0}],
            }
        ],
        "resource_nodes": [{"id": "node_1"}]
    }
    report4 = analyzer.analyze(factory_state_split_stock, goal="balance")
    suggestions4, hints4 = suggestion_tool.generate_suggestions(report4)
    sug_split = next((s for s in suggestions4 if s.id == "inspect_iron_ore_supply_smelter_1"), None)
    # 총량이 80이므로 재고 있음 제안이 발행되어야 함
    assert sug_split is not None
    assert "supply" in sug_split.id


def test_power_grid_machine_condition_workflow() -> None:
    """Validate Sprint 7 machine condition and durability analysis (damaged, broken, maintenance, fallback)."""
    from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
    from agents.process_optimizer.suggestion import OptimizationSuggestionTool
    from agents.process_optimizer.subquest_alert import SubquestAlertBuilder

    analyzer = FactoryStateAnalyzerTool()
    suggestion_tool = OptimizationSuggestionTool()
    alert_builder = SubquestAlertBuilder()

    # 1. durability.ratio가 0.2인 설비 (시나리오 1 - 정비 제안)
    factory_state_low_durability = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "durability": {"current": 20.0, "max": 100.0, "ratio": 0.2},
                "condition": "damaged",
                "maintenance_required": False
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0}
    }

    report1 = analyzer.analyze(factory_state_low_durability, goal="balance")
    assert "smelter_1" in report1.maintenance_required_machines
    assert "smelter_1" not in report1.broken_machines

    suggestions1, hints1 = suggestion_tool.generate_suggestions(report1)
    sug_maint = next((s for s in suggestions1 if s.id == "inspect_machine_condition_smelter_1"), None)
    assert sug_maint is not None
    assert sug_maint.target.id == "smelter_1"
    assert "내구도가 낮아 정비가 필요합니다" in sug_maint.problem
    assert sug_maint.risk == "low"
    assert "smelter_1" in hints1.highlight_targets

    # 2. maintenance_required=True인 설비 (시나리오 2 - 설비 정비 서브퀘스트 발행)
    factory_state_maintenance = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "durability": {"current": 80.0, "max": 100.0, "ratio": 0.8},
                "condition": "normal",
                "maintenance_required": True
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0}
    }

    report2 = analyzer.analyze(factory_state_maintenance, goal="balance")
    assert "smelter_1" in report2.maintenance_required_machines

    alert2 = alert_builder.build_alert(report2, factory_state_maintenance, subquest_mode=True)
    assert alert2.needed is True
    assert alert2.suggested_subquest.title == "설비 정비 수행"
    assert "정비를 수행하세요" in alert2.suggested_subquest.objective
    assert "내구도가 낮아 정비가 필요합니다" in alert2.reason

    # 3. condition="broken"인 설비 (시나리오 3 - 고장 설비 점검 제안 및 서브퀘스트)
    factory_state_broken = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "idle",
                "durability": {"current": 0.0, "max": 100.0, "ratio": 0.0},
                "condition": "broken",
                "maintenance_required": True
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0}
    }

    report3 = analyzer.analyze(factory_state_broken, goal="balance")
    assert "smelter_1" in report3.maintenance_required_machines
    assert "smelter_1" in report3.broken_machines

    suggestions3, hints3 = suggestion_tool.generate_suggestions(report3)
    sug_broken = next((s for s in suggestions3 if s.id == "inspect_machine_condition_smelter_1"), None)
    assert sug_broken is not None
    assert "고장(broken) 상태입니다" in sug_broken.problem
    assert sug_broken.risk == "medium"
    assert "smelter_1" in hints3.highlight_targets

    alert3 = alert_builder.build_alert(report3, factory_state_broken, subquest_mode=True)
    assert alert3.needed is True
    assert alert3.suggested_subquest.title == "고장 설비 점검"
    assert "고장(broken) 상태입니다. 현장으로 가 장비를 점검하고 수리하십시오" in alert3.suggested_subquest.objective
    assert "고장 상태로 멈춰 있습니다" in alert3.reason

    # 3-1. condition="broken"만 있어도 정비 플래그와 내구도 수치와 무관하게 고장 제안이 나와야 한다.
    factory_state_broken_only = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "idle",
                "durability": {"current": 80.0, "max": 100.0, "ratio": 0.8},
                "condition": "broken",
                "maintenance_required": False
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0}
    }

    report_broken_only = analyzer.analyze(factory_state_broken_only, goal="balance")
    assert "smelter_1" in report_broken_only.maintenance_required_machines
    assert "smelter_1" in report_broken_only.broken_machines

    suggestions_broken_only, hints_broken_only = suggestion_tool.generate_suggestions(report_broken_only)
    sug_broken_only = next(
        (s for s in suggestions_broken_only if s.id == "inspect_machine_condition_smelter_1"),
        None,
    )
    assert sug_broken_only is not None
    assert "고장(broken) 상태입니다" in sug_broken_only.problem
    assert sug_broken_only.risk == "medium"
    assert "smelter_1" in hints_broken_only.highlight_targets

    alert_broken_only = alert_builder.build_alert(
        report_broken_only,
        factory_state_broken_only,
        subquest_mode=True,
    )
    assert alert_broken_only.needed is True
    assert alert_broken_only.suggested_subquest.title == "고장 설비 점검"

    # 4. durability 정보가 없는 설비 (시나리오 4 - 레거시/정상 유지)
    factory_state_no_durability = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0}
    }
    report4 = analyzer.analyze(factory_state_no_durability, goal="balance")
    assert not report4.maintenance_required_machines
    assert not report4.broken_machines

    suggestions4, hints4 = suggestion_tool.generate_suggestions(report4)
    sug_none = next((s for s in suggestions4 if "condition" in s.id), None)
    assert sug_none is None


def test_power_grid_missing_state_request_workflow() -> None:
    """Validate Sprint 8 missing state request and need_more_state logic across scenarios."""
    from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
    from agents.process_optimizer.suggestion import OptimizationSuggestionTool
    from agents.process_optimizer.subquest_alert import SubquestAlertBuilder
    from agents.process_optimizer.graph import compile_process_optimizer_graph

    analyzer = FactoryStateAnalyzerTool()
    suggestion_tool = OptimizationSuggestionTool()
    alert_builder = SubquestAlertBuilder()

    # 1. 입력 부족인데 storages가 없거나 비어있는 경우 (시나리오 1 - storage_inventory 요청)
    factory_state_missing_storage = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0},
        # storages 필드가 없음
    }

    report1 = analyzer.analyze(factory_state_missing_storage, goal="balance")
    assert report1.need_more_state is not None
    assert report1.need_more_state["status"] == "need_more_state"
    assert "storage_inventory" in report1.need_more_state["required_state_scopes"]
    assert "storages" in report1.need_more_state["next_request_hint"]["include"]

    # 제안 생성 및 서브퀘스트 생성 우회 검사
    suggestions1, hints1 = suggestion_tool.generate_suggestions(report1)
    assert not suggestions1
    
    alert1 = alert_builder.build_alert(report1, factory_state_missing_storage, subquest_mode=True)
    assert alert1.needed is False

    # 2. 철광석 부족인데 resource_nodes가 없거나 비어있는 경우 (시나리오 2 - resource_nodes 요청)
    factory_state_missing_resource = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0},
        "storages": [], # 빈 storages -> storage_inventory를 먼저 요청
        # resource_nodes 필드가 없음
    }

    report2 = analyzer.analyze(factory_state_missing_resource, goal="balance")
    assert report2.need_more_state is not None
    assert "storage_inventory" in report2.need_more_state["required_state_scopes"]
    assert "storages" in report2.need_more_state["next_request_hint"]["include"]

    # 3. 기계가 idle인데 원인을 모르고 machine_condition 정보(durability 등)가 없는 경우 (시나리오 3 - machine_condition 요청)
    factory_state_missing_condition = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "idle",
                "inputs": [{"item_id": "iron_ore", "amount": 5.0}], # 입력 충분
                "outputs": [{"item_id": "iron_plate", "amount": 0.0, "max_amount": 100.0}], # 출력 비어있음
                "power_consumption": 10.0
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0}, # 전력 충분
        # durability, condition, maintenance_required 없음
    }

    report3 = analyzer.analyze(factory_state_missing_condition, goal="balance")
    assert report3.need_more_state is not None
    assert "machine_condition" in report3.need_more_state["required_state_scopes"]
    assert "machine_condition" in report3.need_more_state["next_request_hint"]["include"]

    # 4. 전력 부족인데 power_grid.nodes가 없거나 비어있는 경우 (시나리오 4 - power_grid 요청)
    factory_state_missing_power_pole = {
        "machines": [],
        "conveyors": [],
        "power_grid": {
            "produced": 10.0,
            "consumed": 50.0,
            "nodes": [], # 비어있는 nodes
            "generators": []
        }
    }

    report4 = analyzer.analyze(factory_state_missing_power_pole, goal="balance")
    assert report4.need_more_state is not None
    assert "power_grid" in report4.need_more_state["required_state_scopes"]
    assert "power_grid" in report4.need_more_state["next_request_hint"]["include"]

    # 5. 모든 정보가 완벽히 존재하여 need_more_state가 유도되지 않는 경우 (시나리오 5 - 정상 분석)
    factory_state_complete = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                "durability": {"current": 90.0, "max": 100.0, "ratio": 0.9},
                "condition": "normal",
                "maintenance_required": False
            }
        ],
        "conveyors": [],
        "power_grid": {
            "produced": 100.0,
            "consumed": 50.0,
            "nodes": [{"id": "power_pole_1"}],
            "generators": []
        },
        "storages": [
            {
                "id": "storage_iron_ore_1",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 200.0}],
            }
        ],
        "resource_nodes": [{"id": "node_iron_ore_1"}]
    }

    report5 = analyzer.analyze(factory_state_complete, goal="balance")
    assert report5.need_more_state is None

    # 6. Graph 파이프라인 연계 및 재분석 검증 (시나리오 6)
    # 6.1 정보 부족하여 need_more_state payload를 리턴하는 그래프 검증
    graph = compile_process_optimizer_graph()
    res1 = graph.invoke({
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 20,
            "factory_state": factory_state_missing_storage
        }
    })
    
    assert res1.get("previewPayload") is not None
    assert res1["previewPayload"]["status"] == "need_more_state"
    assert "storage_inventory" in res1["previewPayload"]["required_state_scopes"]
    assert res1["previewPayload"]["factoryRevision"] == 20

    # 6.2 Unreal이 storages를 준 뒤 재분석을 보낼 시, Sprint 6 로직이 정상 처리되어 preview 제안이 나오는가 검증
    res2 = graph.invoke({
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 21,
            "factory_state": factory_state_complete
        }
    })
    
    assert res2.get("previewPayload") is not None
    assert res2["previewPayload"]["status"] == "preview"
    assert len(res2["previewPayload"]["suggestions"]) > 0


def test_power_grid_resource_nodes_requested_only_when_storage_stock_is_empty() -> None:
    """Verify that resource node requests do not block supply-line suggestions."""
    from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
    from agents.process_optimizer.suggestion import OptimizationSuggestionTool

    analyzer = FactoryStateAnalyzerTool()
    suggestion_tool = OptimizationSuggestionTool()

    factory_state_empty_iron_ore_stock = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0},
        "storages": [
            {
                "id": "storage_iron_ore_1",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
    }

    report_resource = analyzer.analyze(factory_state_empty_iron_ore_stock, goal="balance")

    assert report_resource.need_more_state is not None
    assert "resource_nodes" in report_resource.need_more_state["required_state_scopes"]
    assert "storage_inventory" not in report_resource.need_more_state["required_state_scopes"]
    assert "resource_nodes" in report_resource.need_more_state["next_request_hint"]["include"]
    assert "storages" not in report_resource.need_more_state["next_request_hint"]["include"]

    factory_state_storage_stock_without_resource = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 50.0},
        "storages": [
            {
                "id": "storage_iron_ore_1",
                "type": "storage",
                "inventory": [{"item_id": "iron_ore", "amount": 200.0}],
            }
        ],
    }

    report_supply_line = analyzer.analyze(
        factory_state_storage_stock_without_resource, goal="balance"
    )

    assert report_supply_line.need_more_state is None

    suggestions, hints = suggestion_tool.generate_suggestions(report_supply_line)
    assert any(
        suggestion.id == "inspect_iron_ore_supply_smelter_1"
        for suggestion in suggestions
    )
    assert "smelter_1" in hints.highlight_targets
