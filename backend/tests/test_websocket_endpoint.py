from __future__ import annotations

import logging

import pytest
from fastapi.testclient import TestClient

from app import create_app
from websocket_gateway import gateway


@pytest.fixture(autouse=True)
def disable_llm_for_websocket_tests(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK1_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK2_PROVIDER", "none")


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


def test_agent_websocket_logs_outgoing_response(
    caplog: pytest.LogCaptureFixture,
) -> None:
    caplog.set_level(logging.INFO, logger="uvicorn.error")

    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "request-log-ws",
                    "agent": "process_optimizer",
                    "payload": {"machines": [{"id": "m-1"}]},
                }
            )
            websocket.receive_json()

    assert any(
        record.name == "uvicorn.error"
        and
        "Factory agent WebSocket sending:" in record.message
        and '"request_id": "request-log-ws"' in record.message
        and '"type": "agent.error"' in record.message
        and '"code": "ROUTING_UNAVAILABLE"' in record.message
        for record in caplog.records
    )


def test_sanitize_for_log_masks_sensitive_values() -> None:
    sanitized = gateway.sanitize_for_log(
        {
            "type": "agent.response",
            "payload": {
                "answer": "ok",
                "token": "raw-token",
                "nested": [{"api_key": "raw-key"}],
            },
        }
    )

    assert sanitized == {
        "type": "agent.response",
        "payload": {
            "answer": "ok",
            "token": "[redacted]",
            "nested": [{"api_key": "[redacted]"}],
        },
    }
