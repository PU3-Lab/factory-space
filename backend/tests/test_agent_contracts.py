from __future__ import annotations

import asyncio

import pytest

from factory_space.core.actions.schemas import Action
from factory_space.core.agents.orchestrator import AgentOrchestrator
from factory_space.core.agents.registry import AgentRegistry, UnknownAgentError
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import (
    AgentRequest,
    AgentResponse,
    AgentResponsePayload,
    agent_response_to_envelope,
)


class DummyAgent:
    agent_id = "dummy"

    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        return AgentResponse(
            session_id=context.session_id,
            request_id=context.request_id,
            client_id=context.client_id,
            agent=request.agent,
            payload=AgentResponsePayload(
                text="ok",
                actions=[Action(name="show_ui_message", args={"text": "ok"})],
            ),
        )


def test_orchestrator_dispatches_registered_agent() -> None:
    registry = AgentRegistry()
    registry.register(DummyAgent())
    orchestrator = AgentOrchestrator(registry)

    response = asyncio.run(
        orchestrator.process(
            AgentRequest(
                session_id="session-1",
                request_id="request-1",
                client_id="client-1",
                agent="dummy",
                payload={"message": "hello"},
            )
        )
    )

    assert response.agent == "dummy"
    assert response.session_id == "session-1"
    assert response.payload.text == "ok"
    assert response.payload.actions[0].name == "show_ui_message"


def test_registry_raises_for_unknown_agent() -> None:
    registry = AgentRegistry()

    with pytest.raises(UnknownAgentError):
        registry.get("missing")


def test_agent_response_converts_to_message_envelope() -> None:
    response = AgentResponse(
        session_id="session-1",
        request_id="request-1",
        agent="dummy",
        payload=AgentResponsePayload(
            text="hello",
            actions=[Action(name="highlight_object", args={"object_id": "a"})],
        ),
    )

    envelope = agent_response_to_envelope(response)

    assert envelope.type == "agent_response"
    assert envelope.payload["text"] == "hello"
    assert envelope.payload["actions"][0]["name"] == "highlight_object"
