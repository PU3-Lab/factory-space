from __future__ import annotations

from fastapi.testclient import TestClient

from app import create_app


def test_health_endpoint_returns_ok() -> None:
    client = TestClient(create_app())

    response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_agent_websocket_runs_pipeline_for_explicit_agent() -> None:
    client = TestClient(create_app())

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

    assert response["type"] == "agent.response"
    assert response["agent"] == "process_optimizer"


def test_agent_websocket_returns_error_for_malformed_envelope() -> None:
    client = TestClient(create_app())

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
    client = TestClient(create_app())

    with client.websocket_connect("/ws/agent") as websocket:
        websocket.send_text("{not-json")
        response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["error"]["code"] == "INVALID_JSON"
