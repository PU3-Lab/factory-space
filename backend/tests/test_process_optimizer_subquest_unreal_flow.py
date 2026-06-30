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
        "storages": [
            {
                "id": "warehouse_1",
                "inventory": [
                    {
                        "item_id": "iron_ore",
                        "amount": 100.0,
                        "max_amount": 500.0,
                    }
                ],
            }
        ],
        "power_grid": {"produced": 120.0, "consumed": 90.0},
    }


def test_subquest_check_requests_a_factory_snapshot() -> None:
    """subquest_check asks Unreal for the state needed to judge a subquest."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-check",
            "session_id": "session-subquest-check",
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "subquest_check",
                "goal": "balance",
            },
        }
    )

    assert response["type"] == "agent.response"
    payload = response["payload"]
    assert payload["status"] == "need_more_state"
    assert payload["snapshot_request_id"]
    assert payload["required_state_scopes"] == [
        "machines",
        "machine_condition",
        "storages",
        "conveyors",
        "power_grid",
        "resource_nodes",
    ]
    assert payload["next_request_hint"] == {
        "agent": "process_optimizer",
        "operation": "state_update",
        "request_source": "subquest_check",
        "snapshot_request_id": payload["snapshot_request_id"],
    }


def test_subquest_snapshot_returns_a_subquest_candidate() -> None:
    """A correlated state_update returns the candidate requested by Unreal."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))
    session_id = "session-subquest-snapshot"

    check_response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-check-2",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "subquest_check",
                "goal": "balance",
            },
        }
    )
    snapshot_request_id = check_response["payload"]["snapshot_request_id"]

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-snapshot",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "state_update",
                "request_source": "subquest_check",
                "snapshot_request_id": snapshot_request_id,
                "goal": "balance",
                "factoryRevision": 42,
                "factory_state": _input_shortage_factory_state(),
            },
        }
    )

    assert response["type"] == "agent.response"
    payload = response["payload"]
    assert payload["status"] == "success"
    assert payload["snapshot_request_id"] == snapshot_request_id
    assert payload["optimization_alert"]["needed"] is True
    assert payload["optimization_alert"]["target"] == {
        "type": "machine",
        "id": "smelter_1",
    }


def test_plain_state_update_stores_state_without_creating_a_subquest() -> None:
    """A periodic state update does not create a candidate unless requested."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-plain-state-update",
            "session_id": "session-plain-state-update",
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "state_update",
                "goal": "balance",
                "factoryRevision": 42,
                "factory_state": _input_shortage_factory_state(),
            },
        }
    )

    assert response["type"] == "agent.response"
    assert response["payload"]["status"] == "success"
    assert response["payload"]["optimization_alert"]["needed"] is False


def test_subquest_snapshot_requires_snapshot_request_id() -> None:
    """A subquest snapshot without its correlation ID is rejected."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-snapshot-without-id",
            "session_id": "session-subquest-snapshot-without-id",
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "state_update",
                "request_source": "subquest_check",
                "factoryRevision": 42,
                "factory_state": _input_shortage_factory_state(),
            },
        }
    )

    assert response["type"] == "agent.error"
    assert response["error"]["code"] == "INVALID_REQUEST_PAYLOAD"
    assert "snapshot_request_id" in response["error"]["message"]


def test_subquest_snapshot_rejects_unknown_snapshot_request_id() -> None:
    """A snapshot ID that the backend did not issue is rejected."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-snapshot-unknown-id",
            "session_id": "session-subquest-snapshot-unknown-id",
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "state_update",
                "request_source": "subquest_check",
                "snapshot_request_id": "snapshot-not-issued",
                "factoryRevision": 42,
                "factory_state": _input_shortage_factory_state(),
            },
        }
    )

    assert response["type"] == "agent.error"
    assert response["error"]["code"] == "INVALID_SNAPSHOT_REQUEST"


def test_subquest_snapshot_preserves_request_id_when_more_state_is_needed() -> None:
    """A partial snapshot keeps the same request ID in the next state hint."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))
    session_id = "session-subquest-more-state"

    check_response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-more-state-check",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "subquest_check",
                "goal": "balance",
            },
        }
    )
    snapshot_request_id = check_response["payload"]["snapshot_request_id"]

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "req-subquest-more-state-snapshot",
            "session_id": session_id,
            "client_id": "unreal",
            "agent": "process_optimizer",
            "payload": {
                "operation": "state_update",
                "request_source": "subquest_check",
                "snapshot_request_id": snapshot_request_id,
                "factoryRevision": 42,
                "factory_state": {
                    "machines": [{"id": "smelter_1", "status": "idle"}],
                    "conveyors": [],
                    "storages": [],
                    "resource_nodes": [],
                    "power_grid": {
                        "produced": 100.0,
                        "consumed": 10.0,
                        "nodes": [],
                        "generators": [],
                    },
                },
            },
        }
    )

    assert response["type"] == "agent.response"
    payload = response["payload"]
    assert payload["status"] == "need_more_state"
    assert "machine_condition" in payload["required_state_scopes"]
    assert payload["next_request_hint"]["request_source"] == "subquest_check"
    assert (
        payload["next_request_hint"]["snapshot_request_id"]
        == snapshot_request_id
    )


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
