from __future__ import annotations

import asyncio
import json

from factory_space.messages.protocol import ErrorMessage, MessageEnvelope
from factory_space.websocket.endpoint import handle_raw_message


def test_handle_raw_message_returns_pong() -> None:
    response = asyncio.run(
        handle_raw_message(
            json.dumps(
                {
                    "type": "ping",
                    "version": "1.0",
                    "session_id": "session-1",
                    "payload": {"timestamp": "now"},
                }
            ),
            client_id="client-1",
        )
    )

    assert isinstance(response, MessageEnvelope)
    assert response.type == "pong"
    assert response.client_id == "client-1"


def test_handle_raw_message_dispatches_agent_request() -> None:
    response = asyncio.run(
        handle_raw_message(
            json.dumps(
                {
                    "type": "agent_request",
                    "version": "1.0",
                    "session_id": "session-1",
                    "agent": "qa_chatbot",
                    "payload": {"question": "hello"},
                }
            ),
            client_id="client-1",
        )
    )

    assert isinstance(response, MessageEnvelope)
    assert response.type == "agent_response"
    assert response.agent == "qa_chatbot"
    assert response.payload["metadata"]["status"] == "stub"


def test_handle_raw_message_returns_error_for_invalid_json() -> None:
    response = asyncio.run(handle_raw_message("{", client_id="client-1"))

    assert isinstance(response, ErrorMessage)
    assert response.type == "error"
    assert response.payload.code == "INVALID_JSON"
