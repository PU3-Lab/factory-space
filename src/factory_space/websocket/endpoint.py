"""WebSocket endpoint."""

from __future__ import annotations

import json

from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from pydantic import ValidationError

from factory_space.core.agents.orchestrator import AgentOrchestrator
from factory_space.core.agents.registry import create_default_registry
from factory_space.messages.protocol import ErrorMessage, ErrorPayload, MessageEnvelope
from factory_space.messages.router import MessageRouter
from factory_space.websocket.manager import WebSocketConnectionManager

router = APIRouter()

connection_manager = WebSocketConnectionManager()
message_router = MessageRouter(
    AgentOrchestrator(create_default_registry()),
)


@router.websocket("/ws/{client_id}")
async def websocket_endpoint(websocket: WebSocket, client_id: str) -> None:
    """Handle Unreal WebSocket messages."""

    await connection_manager.connect(client_id, websocket)
    try:
        while True:
            raw_message = await websocket.receive_text()
            response = await handle_raw_message(raw_message, client_id)
            await websocket.send_json(response.model_dump(mode="json"))
    except WebSocketDisconnect:
        connection_manager.disconnect(client_id)


async def handle_raw_message(
    raw_message: str,
    client_id: str,
) -> MessageEnvelope | ErrorMessage:
    """Parse and route one raw WebSocket text message."""

    try:
        data = json.loads(raw_message)
    except json.JSONDecodeError as error:
        return _transport_error(
            client_id=client_id,
            code="INVALID_JSON",
            message="JSON 파싱에 실패했습니다.",
            details={"error": str(error)},
        )

    if not isinstance(data, dict):
        return _transport_error(
            client_id=client_id,
            code="INVALID_MESSAGE",
            message="메시지는 JSON object여야 합니다.",
        )

    data.setdefault("client_id", client_id)

    try:
        envelope = MessageEnvelope.model_validate(data)
    except ValidationError as error:
        return _transport_error(
            client_id=client_id,
            code="INVALID_MESSAGE",
            message="메시지 envelope 검증에 실패했습니다.",
            details={"errors": error.errors()},
        )

    return await message_router.route(envelope)


def _transport_error(
    *,
    client_id: str,
    code: str,
    message: str,
    details: dict[str, object] | None = None,
) -> ErrorMessage:
    return ErrorMessage(
        session_id="unknown",
        client_id=client_id,
        payload=ErrorPayload(
            code=code,
            message=message,
            details=details or {},
        ),
    )
