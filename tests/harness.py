from __future__ import annotations

import asyncio
from dataclasses import dataclass, field
from typing import Any

from factory_space.core.actions.schemas import Action
from factory_space.core.agents.orchestrator import AgentOrchestrator
from factory_space.core.agents.registry import create_default_registry
from factory_space.messages.protocol import ErrorMessage, MessageEnvelope
from factory_space.messages.router import MessageRouter


@dataclass(frozen=True)
class AgentScenario:
    """Scenario input for one agent contract check."""

    name: str
    agent: str
    payload: dict[str, Any]
    session_id: str = "smoke-session"
    client_id: str = "smoke-client"
    request_id: str | None = None
    version: str = "1.0"
    expected_action_names: tuple[str, ...] = field(default_factory=tuple)


def run_agent_scenario(scenario: AgentScenario) -> MessageEnvelope | ErrorMessage:
    """Run an agent request through the real message router."""

    request_id = scenario.request_id or f"req-{scenario.agent}-smoke"
    envelope = MessageEnvelope(
        type="agent_request",
        version=scenario.version,
        request_id=request_id,
        session_id=scenario.session_id,
        client_id=scenario.client_id,
        agent=scenario.agent,
        payload=scenario.payload,
    )

    return _route(envelope)


def run_ping_scenario(
    *,
    payload: dict[str, Any] | None = None,
    session_id: str = "smoke-session",
    client_id: str = "smoke-client",
    request_id: str = "req-ping-smoke",
) -> MessageEnvelope | ErrorMessage:
    """Run a ping through the real message router."""

    envelope = MessageEnvelope(
        type="ping",
        request_id=request_id,
        session_id=session_id,
        client_id=client_id,
        payload=payload or {},
    )

    return _route(envelope)


def assert_agent_response_contract(
    response: MessageEnvelope,
    *,
    expected_agent: str,
    expected_action_names: tuple[str, ...] = (),
) -> None:
    """Assert the stable Unreal-facing agent response shape."""

    assert response.type == "agent_response"
    assert response.version == "1.0"
    assert response.session_id
    assert response.client_id
    assert response.request_id
    assert response.agent == expected_agent

    assert isinstance(response.payload, dict)
    assert isinstance(response.payload.get("text"), str)
    assert isinstance(response.payload.get("metadata"), dict)

    actions = response.payload.get("actions")
    assert isinstance(actions, list)
    assert actions, "agent_response payload.actions must include at least one action"

    action_names: list[str] = []
    for action_payload in actions:
        action = Action.model_validate(action_payload)
        action_names.append(action.name)

    for expected_action_name in expected_action_names:
        assert expected_action_name in action_names


def assert_error_contract(
    response: ErrorMessage,
    *,
    expected_code: str,
) -> None:
    """Assert the stable Unreal-facing error response shape."""

    assert response.type == "error"
    assert response.version == "1.0"
    assert response.session_id
    assert response.payload.code == expected_code
    assert response.payload.message
    assert isinstance(response.payload.details, dict)


def _route(envelope: MessageEnvelope) -> MessageEnvelope | ErrorMessage:
    router = MessageRouter(AgentOrchestrator(create_default_registry()))
    return asyncio.run(router.route(envelope))
