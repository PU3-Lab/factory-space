"""WebSocket message protocol models."""

from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field

from factory_space.core.actions.schemas import Action, ActionResult

MessageType = Literal[
    "ping",
    "pong",
    "agent_request",
    "agent_response",
    "action_result",
    "error",
]


class MessageEnvelope(BaseModel):
    """Common fields shared by all WebSocket messages."""

    model_config = ConfigDict(extra="forbid")

    type: MessageType
    version: str = "1.0"
    session_id: str
    request_id: str | None = None
    client_id: str | None = None
    agent: str | None = None
    payload: dict[str, Any] = Field(default_factory=dict)


class AgentRequest(BaseModel):
    """Normalized request passed from the router/orchestrator to an agent."""

    model_config = ConfigDict(extra="forbid")

    version: str = "1.0"
    session_id: str
    agent: str
    payload: dict[str, Any] = Field(default_factory=dict)
    request_id: str | None = None
    client_id: str | None = None


class AgentResponsePayload(BaseModel):
    """Payload returned by an agent."""

    model_config = ConfigDict(extra="forbid")

    text: str = ""
    actions: list[Action] = Field(default_factory=list)
    metadata: dict[str, Any] = Field(default_factory=dict)


class AgentResponse(BaseModel):
    """Normalized response returned by every agent."""

    model_config = ConfigDict(extra="forbid")

    type: Literal["agent_response"] = "agent_response"
    version: str = "1.0"
    session_id: str
    agent: str
    payload: AgentResponsePayload = Field(default_factory=AgentResponsePayload)
    request_id: str | None = None
    client_id: str | None = None


class ActionResultMessage(BaseModel):
    """Message sent by Unreal after executing an action."""

    model_config = ConfigDict(extra="forbid")

    type: Literal["action_result"] = "action_result"
    version: str = "1.0"
    session_id: str
    agent: str | None = None
    payload: ActionResult
    request_id: str | None = None
    client_id: str | None = None


class ErrorPayload(BaseModel):
    """Structured error payload."""

    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    details: dict[str, Any] = Field(default_factory=dict)


class ErrorMessage(BaseModel):
    """Error message returned to Unreal."""

    model_config = ConfigDict(extra="forbid")

    type: Literal["error"] = "error"
    version: str = "1.0"
    session_id: str
    payload: ErrorPayload
    request_id: str | None = None
    client_id: str | None = None
    agent: str | None = None


def agent_response_to_envelope(response: AgentResponse) -> MessageEnvelope:
    """Convert an agent response into a transport envelope."""

    return MessageEnvelope(
        type=response.type,
        version=response.version,
        session_id=response.session_id,
        request_id=response.request_id,
        client_id=response.client_id,
        agent=response.agent,
        payload=response.payload.model_dump(),
    )
