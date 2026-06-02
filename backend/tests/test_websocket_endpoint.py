from __future__ import annotations

from fastapi.testclient import TestClient

from app import create_app


def test_health_endpoint_returns_ok() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_agent_websocket_requires_top_level_routing_model_for_agent_request() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "request-ws",
                    "agent": "process_optimizer",
                    "payload": {"machines": [{"id": "m-1"}]},
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["agent"] == "process_optimizer"
    assert response["error"]["code"] == "ROUTING_UNAVAILABLE"


def test_agent_websocket_returns_error_for_malformed_envelope() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "wrong.type",
                    "request_id": "request-invalid-ws-envelope",
                    "payload": {},
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["error"]["code"] == "INVALID_ENVELOPE"


def test_agent_websocket_returns_error_for_invalid_json() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_text("{not-json")
            response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["error"]["code"] == "INVALID_JSON"


def test_agent_websocket_preserves_unreal_correlation_fields_on_error() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "unreal-smoke-1",
                    "session_id": "dev-session",
                    "client_id": "unreal-client",
                    "agent": "process_optimizer",
                    "payload": {"machines": [{"id": "assembler-1"}]},
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["request_id"] == "unreal-smoke-1"
    assert response["session_id"] == "dev-session"
    assert response["client_id"] == "unreal-client"
    assert response["agent"] == "process_optimizer"
    assert response["error"]["code"] == "ROUTING_UNAVAILABLE"
