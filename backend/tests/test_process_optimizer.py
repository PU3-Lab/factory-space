"""Tests for process optimizer agent."""

from __future__ import annotations

import pytest
from pydantic import ValidationError

from agents.base import AgentContext
from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.agent import ProcessOptimizerAgent
from agents.process_optimizer.schemas import ProcessOptimizerPayload
from agents.process_optimizer.session_memory import process_optimizer_memory


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


def test_process_optimizer_analyze_fallback_or_mock() -> None:
    """Ensure analyze request correctly sets up and fallback returns suggestion structure."""
    pipeline = AgentPipeline(llm=StubLLM(["process_optimizer", None]))
    session_id = "session-analyze-test"
    process_optimizer_memory.clear(session_id)

    # Put state in memory beforehand
    state = {"machines": [{"id": "m1"}]}
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
    assert "summary" in res["payload"]
    assert "suggestions" in res["payload"]
    # Confirm it referenced the 1 machine in memory
    assert "1개 설비" in res["payload"]["summary"]


def test_process_optimizer_analyze_defaults_when_operation_omitted() -> None:
    """operation을 생략하면 ProcessOptimizerPayload 기본값처럼 analyze 요청으로 처리합니다."""
    pipeline = AgentPipeline(llm=StubLLM(["process_optimizer", None]))
    session_id = "session-analyze-default-operation"
    process_optimizer_memory.clear(session_id)
    process_optimizer_memory.update(session_id, {"machines": [{"id": "m1"}]}, 10)

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
    assert res["payload"]["status"] == "suggestion"
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
    assert res["payload"]["status"] == "suggestion"
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
    assert "수석 매니저" in prompt
    assert "m1" in prompt
    assert "throughput" in prompt
    assert "15" in prompt

    result = agent.fallback({}, context)
    assert result.agent == "process_optimizer"
    assert "1개 설비" in result.payload["summary"]


def test_unreal_websocket_contract_sample_validation() -> None:
    """Unreal 계약 샘플 JSON이 실제 Pydantic 스키마 및 검증 모델을 정상 통과하는지 확인합니다."""
    import json
    import os

    sample_path = "c:/factory-space/docs/process_optimizer/agent_test_sample.json"
    assert os.path.exists(sample_path)

    with open(sample_path, encoding="utf-8") as f:
        data = json.load(f)

    # Envelope 구조 간접 체크
    assert data["type"] == "agent.request"
    assert data["agent"] == "process_optimizer"

    # 1. Payload 검증
    payload_data = data["payload"]
    payload_obj = ProcessOptimizerPayload.model_validate(payload_data)
    assert payload_obj.operation == "analyze"
    assert payload_obj.goal == "congestion_relief"

    # 2. Payload 내의 factoryRevision 및 factory_state 구조 검증
    assert payload_data["factoryRevision"] == 12

    from agents.process_optimizer.schemas import FactoryState

    state_data = payload_data["factory_state"]
    state_obj = FactoryState.model_validate(state_data)

    # 세부 필드들이 Pydantic 기본값과 extra 파싱을 통해 정확하게 로드되었는지 검증
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
