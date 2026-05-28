"""Message router."""

from __future__ import annotations

from pydantic import ValidationError

from factory_space.core.agents.orchestrator import AgentOrchestrator
from factory_space.core.agents.registry import UnknownAgentError
from factory_space.messages.protocol import (
    AgentRequest,
    ErrorMessage,
    ErrorPayload,
    MessageEnvelope,
    agent_response_to_envelope,
)


class MessageRouter:
    """Route normalized transport envelopes to backend handlers."""

    def __init__(self, orchestrator: AgentOrchestrator) -> None:
        self._orchestrator = orchestrator

    async def route(self, envelope: MessageEnvelope) -> MessageEnvelope | ErrorMessage:
        """Route one WebSocket message envelope."""

        if envelope.type == "ping":
            return MessageEnvelope(
                type="pong",
                version=envelope.version,
                session_id=envelope.session_id,
                request_id=envelope.request_id,
                client_id=envelope.client_id,
                payload=envelope.payload,
            )

        if envelope.type == "agent_request":
            return await self._route_agent_request(envelope)

        if envelope.type == "action_result":
            return MessageEnvelope(
                type="pong",
                version=envelope.version,
                session_id=envelope.session_id,
                request_id=envelope.request_id,
                client_id=envelope.client_id,
                agent=envelope.agent,
                payload={"status": "received"},
            )

        return self._error(
            envelope,
            code="UNKNOWN_MESSAGE_TYPE",
            message=f"지원하지 않는 message type입니다: {envelope.type}",
        )

    async def _route_agent_request(
        self,
        envelope: MessageEnvelope,
    ) -> MessageEnvelope | ErrorMessage:
        if envelope.agent is None:
            return self._error(
                envelope,
                code="VALIDATION_ERROR",
                message="agent_request에는 agent field가 필요합니다.",
            )

        try:
            request = AgentRequest(
                version=envelope.version,
                session_id=envelope.session_id,
                request_id=envelope.request_id,
                client_id=envelope.client_id,
                agent=envelope.agent,
                payload=envelope.payload,
            )
            response = await self._orchestrator.process(request)
        except ValidationError as error:
            return self._error(
                envelope,
                code="VALIDATION_ERROR",
                message="agent request 검증에 실패했습니다.",
                details={"errors": error.errors()},
            )
        except UnknownAgentError as error:
            return self._error(
                envelope,
                code="UNKNOWN_AGENT",
                message="요청한 agent를 찾을 수 없습니다.",
                details={"agent": error.agent_id},
            )

        return agent_response_to_envelope(response)

    def _error(
        self,
        envelope: MessageEnvelope,
        *,
        code: str,
        message: str,
        details: dict[str, object] | None = None,
    ) -> ErrorMessage:
        return ErrorMessage(
            version=envelope.version,
            session_id=envelope.session_id,
            request_id=envelope.request_id,
            client_id=envelope.client_id,
            agent=envelope.agent,
            payload=ErrorPayload(
                code=code,
                message=message,
                details=details or {},
            ),
        )
