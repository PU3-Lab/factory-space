"""Sprint 3 tests for the Process Optimizer subquest handoff flow."""

from __future__ import annotations

from typing import Any

from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.session_memory import process_optimizer_memory


class StubLLM:
    """Return predefined responses so tests do not depend on a live LLM."""

    def __init__(self, responses: list[str | None]) -> None:
        self.responses = responses
        self.calls = 0

    def invoke(self, prompt: str) -> str | None:
        if self.calls >= len(self.responses):
            return None
        response = self.responses[self.calls]
        self.calls += 1
        return response


def _input_shortage_factory_state() -> dict[str, Any]:
    return {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "operating_rate": 0.2,
                "inputs": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}],
                "outputs": [],
                "power_consumption": 15.0,
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 120.0, "consumed": 90.0},
    }


def test_state_update_returns_unreal_subquest_contract() -> None:
    """state_update returns a suggested subquest that Unreal can present."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))
    session_id = "session-subquest-contract"
    process_optimizer_memory.clear(session_id)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-contract",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "state_update",
                "goal": "balance",
                "factoryRevision": 42,
                "subquest_mode": True,
                "factory_state": _input_shortage_factory_state(),
            },
        }
    )

    assert response["type"] == "agent.response"
    payload = response["payload"]
    assert payload["status"] == "success"

    alert = payload["optimization_alert"]
    assert alert["needed"] is True
    assert alert["severity"] == "medium"
    assert alert["target"] == {"type": "machine", "id": "smelter_1"}

    subquest = alert["suggested_subquest"]
    assert subquest["target"] == {"type": "machine", "id": "smelter_1"}
    assert subquest["severity"] == "medium"
    assert subquest["next_request"] == {
        "agent": "process_optimizer",
        "operation": "analyze",
        "goal": "balance",
        "request_source": "subquest",
        "target": {"type": "machine", "id": "smelter_1"},
    }

    assert "commands" not in payload
    assert "plan_id" not in payload
    assert "changes" not in payload


def test_state_update_subquest_next_request_can_continue_to_analyze_and_apply() -> None:
    """Unreal can combine next_request with the latest snapshot and continue the flow."""
    pipeline = AgentPipeline(llm=StubLLM([None, None, None]))
    session_id = "session-subquest-smoke"
    process_optimizer_memory.clear(session_id)
    factory_state = _input_shortage_factory_state()

    state_response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-state",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "state_update",
                "goal": "balance",
                "factoryRevision": 42,
                "subquest_mode": True,
                "factory_state": factory_state,
            },
        }
    )

    next_request = dict(
        state_response["payload"]["optimization_alert"]["suggested_subquest"][
            "next_request"
        ]
    )
    next_request["factoryRevision"] = 43
    next_request["factory_state"] = factory_state

    analyze_response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-analyze",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": next_request.pop("agent"),
            "payload": next_request,
        }
    )

    assert analyze_response["type"] == "agent.response"
    analyze_payload = analyze_response["payload"]
    assert analyze_payload["status"] == "preview"
    assert analyze_payload["factoryRevision"] == 43
    assert analyze_payload["changes"][0]["target"]["id"] == "smelter_1"
    assert "commands" not in analyze_payload

    apply_response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-apply",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "apply",
                "plan_id": analyze_payload["plan_id"],
                "factoryRevision": 43,
                "approval": True,
                "approved_change_ids": [analyze_payload["changes"][0]["id"]],
                "factory_state": factory_state,
            },
        }
    )

    assert apply_response["type"] == "agent.response"
    apply_payload = apply_response["payload"]
    assert apply_payload["status"] == "execute_ready"
    assert apply_payload["commands"]
