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


@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket) -> None:
    """Handle Unreal WebSocket messages on a single channel."""

    await websocket.accept()
    client_id: str | None = None
    try:
        while True:
            raw_message = await websocket.receive_text()

            # Extract client_id from first message if not yet identified
            if client_id is None:
                extracted_id = await _extract_and_register_client(
                    raw_message, websocket
                )
                if extracted_id is None:
                    continue
                client_id = extracted_id

            response = await handle_raw_message(raw_message, client_id)
            await websocket.send_json(response.model_dump(mode="json"))
    except WebSocketDisconnect:
        if client_id:
            connection_manager.disconnect(client_id)


async def _extract_and_register_client(
    raw_message: str,
    websocket: WebSocket,
) -> str | None:
    """Extract client_id from first message and register the connection."""

    try:
        data = json.loads(raw_message)
    except json.JSONDecodeError as error:
        response = _transport_error(
            client_id="unknown",
            code="INVALID_JSON",
            message="JSON 파싱에 실패했습니다.",
            details={"error": str(error)},
        )
        await websocket.send_json(response.model_dump(mode="json"))
        return None

    if not isinstance(data, dict):
        response = _transport_error(
            client_id="unknown",
            code="INVALID_MESSAGE",
            message="메시지는 JSON object여야 합니다.",
        )
        await websocket.send_json(response.model_dump(mode="json"))
        return None

    if "client_id" not in data:
        response = _transport_error(
            client_id="unknown",
            code="MISSING_CLIENT_ID",
            message="client_id 필드가 필요합니다.",
        )
        await websocket.send_json(response.model_dump(mode="json"))
        return None

    client_id = data["client_id"]
    await connection_manager.connect(client_id, websocket)
    return client_id


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

    # Ensure client_id is set
    data["client_id"] = client_id

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
