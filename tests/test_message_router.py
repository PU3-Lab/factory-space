from __future__ import annotations

import asyncio

from factory_space.core.agents.orchestrator import AgentOrchestrator
from factory_space.core.agents.registry import create_default_registry
from factory_space.messages.protocol import ErrorMessage, MessageEnvelope
from factory_space.messages.router import MessageRouter


def test_default_registry_contains_initial_agents() -> None:
    registry = create_default_registry()

    assert registry.list_agent_ids() == [
        "factory_optimization",
        "material_generation",
        "qa_chatbot",
        "quest",
    ]


def test_router_returns_pong_for_ping() -> None:
    router = MessageRouter(AgentOrchestrator(create_default_registry()))

    response = asyncio.run(
        router.route(
            MessageEnvelope(
                type="ping",
                session_id="session-1",
                request_id="request-1",
                client_id="client-1",
                payload={"timestamp": "now"},
            )
        )
    )

    assert isinstance(response, MessageEnvelope)
    assert response.type == "pong"
    assert response.payload == {"timestamp": "now"}


def test_router_dispatches_agent_request() -> None:
    router = MessageRouter(AgentOrchestrator(create_default_registry()))

    response = asyncio.run(
        router.route(
            MessageEnvelope(
                type="agent_request",
                session_id="session-1",
                request_id="request-1",
                client_id="client-1",
                agent="quest",
                payload={"event": "player_entered_area"},
            )
        )
    )

    assert isinstance(response, MessageEnvelope)
    assert response.type == "agent_response"
    assert response.agent == "quest"
    assert response.payload["metadata"]["status"] == "stub"
    assert response.payload["actions"][0]["name"] == "show_ui_message"


def test_router_returns_error_for_unknown_agent() -> None:
    router = MessageRouter(AgentOrchestrator(create_default_registry()))

    response = asyncio.run(
        router.route(
            MessageEnvelope(
                type="agent_request",
                session_id="session-1",
                agent="missing",
                payload={},
            )
        )
    )

    assert isinstance(response, ErrorMessage)
    assert response.type == "error"
    assert response.payload.code == "UNKNOWN_AGENT"
