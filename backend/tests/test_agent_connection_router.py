from __future__ import annotations

from fastapi.testclient import TestClient

from app import create_app


def test_agent_connection_manifest_exposes_unreal_connection_contract() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/api/v1/agent-connection")

    assert response.status_code == 200
    body = response.json()
    assert body["status"] == "ok"
    assert body["health_path"] == "/health"
    assert body["websocket_path"] == "/ws/agent"
    assert body["request_type"] == "agent.request"
    assert body["response_types"] == ["agent.response", "agent.error"]
    assert body["sample_request"] == {
        "type": "agent.request",
        "request_id": "unreal-smoke-1",
        "session_id": "dev-session",
        "client_id": "unreal-client",
        "agent": "process_optimizer",
        "payload": {"machines": [{"id": "assembler-1"}]},
    }


def test_agent_connection_manifest_lists_supported_agent_ids() -> None:
    with TestClient(create_app()) as client:
        body = client.get("/api/v1/agent-connection").json()

    assert body["top_level_agents"] == [
        "process_optimizer",
        "operator_guide",
        "quest_generator",
        "new_material_generator",
    ]
    assert body["leaf_agents"] == {
        "process_optimizer": ["process_optimizer"],
        "operator_guide": [
            "operator_guide.recipe_explainer",
            "operator_guide.machine_help",
            "operator_guide.troubleshooter",
        ],
        "quest_generator": [
            "quest_generator.production_quest",
            "quest_generator.economy_quest",
        ],
        "new_material_generator": ["new_material_generator"],
    }
