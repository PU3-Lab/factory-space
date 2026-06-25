from __future__ import annotations

import logging
from collections.abc import Callable
from typing import Any

import pytest
from fastapi.testclient import TestClient

from agents.pipeline import AgentPipeline
from app import create_app
from tests.harness import StubLLM
from websocket_gateway import gateway


@pytest.fixture(autouse=True)
def disable_llm_for_websocket_tests(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK1_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK2_PROVIDER", "none")


def _process_optimizer_analyze_payload(machine_id: str = "smelter_1") -> dict[str, Any]:
    return {
        "operation": "analyze",
        "goal": "balance",
        "factoryRevision": 1,
        "factory_state": {
            "machines": [
                {
                    "id": machine_id,
                    "type": "smelter",
                    "status": "operating",
                    "operating_rate": 0.2,
                    "inputs": [
                        {
                            "item_id": "iron_ore",
                            "amount": 0.0,
                            "max_amount": 100.0,
                        }
                    ],
                }
            ],
            "conveyors": [],
            "power_grid": {"produced": 100.0, "consumed": 50.0},
        },
    }


def test_health_endpoint_returns_ok() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_agent_websocket_returns_process_optimizer_response_without_llm() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "request-ws",
                    "agent": "process_optimizer",
                    "payload": _process_optimizer_analyze_payload("m-1"),
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.response"
    assert response["agent"] == "process_optimizer"
    assert response["payload"]["status"] == "preview"
    assert response["payload"]["metadata"]["selectedAgent"] == "process_optimizer"


def test_agent_websocket_accepts_process_optimizer_state_update() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "request-ws-state-update",
                    "session_id": "dev-session-state-update",
                    "client_id": "unreal-client",
                    "agent": "process_optimizer",
                    "payload": {
                        "operation": "state_update",
                        "goal": "balance",
                        "factoryRevision": 42,
                        "factory_state": {
                            "machines": [],
                            "conveyors": [],
                            "power_grid": {"produced": 100.0, "consumed": 40.0},
                        },
                    },
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.response"
    assert response["request_id"] == "request-ws-state-update"
    assert response["session_id"] == "dev-session-state-update"
    assert response["client_id"] == "unreal-client"
    assert response["agent"] == "process_optimizer"
    assert response["payload"]["status"] == "success"
    assert response["payload"]["factoryRevision"] == 42


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


def test_agent_websocket_returns_error_when_pipeline_raises() -> None:
    class _RaisingPipeline:
        def run(
            self,
            message: dict[str, Any],
            on_progress: Callable[[str, str], None] | None = None,
        ) -> dict[str, Any]:
            raise RuntimeError("pipeline failed")

    with TestClient(create_app()) as client:
        client.app.state.agent_pipeline = _RaisingPipeline()

        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "request-pipeline-failure",
                    "session_id": "test-session",
                    "client_id": "test-client",
                    "agent": "operator_guide",
                    "payload": {"question": "분쇄기가 뭐야?"},
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["request_id"] == "request-pipeline-failure"
    assert response["session_id"] == "test-session"
    assert response["client_id"] == "test-client"
    assert response["agent"] == "operator_guide"
    assert response["error"]["code"] == "AGENT_PIPELINE_ERROR"


def test_agent_websocket_preserves_unreal_correlation_fields_on_response() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "unreal-smoke-1",
                    "session_id": "dev-session",
                    "client_id": "unreal-client",
                    "agent": "process_optimizer",
                    "payload": _process_optimizer_analyze_payload("assembler-1"),
                }
            )
            response = websocket.receive_json()

    assert response["type"] == "agent.response"
    assert response["request_id"] == "unreal-smoke-1"
    assert response["session_id"] == "dev-session"
    assert response["client_id"] == "unreal-client"
    assert response["agent"] == "process_optimizer"
    assert response["payload"]["status"] == "preview"


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
                    "payload": _process_optimizer_analyze_payload("m-1"),
                }
            )
            websocket.receive_json()

    assert any(
        record.name == "uvicorn.error"
        and "Factory agent WebSocket sending:" in record.message
        and '"request_id": "request-log-ws"' in record.message
        and '"type": "agent.response"' in record.message
        and '"status": "preview"' in record.message
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


def test_agent_websocket_streams_progress_for_operator_guide() -> None:
    """operator_guide 요청에서 WebSocket progress가 중복 없이 순서대로 오는지 확인한다.

    초보자용 설명:
        실제 agent-test 화면은 `/ws/agent` WebSocket으로 진행 메시지를 받습니다.
        이 테스트는 최종 답변 전 `agent.progress`가 정확히 한 번씩만 도착하는지
        확인해 같은 말풍선이 반복되는 문제를 막습니다.
    """
    llm = StubLLM([None])
    pipeline = AgentPipeline(llm=llm)

    with TestClient(create_app()) as client:
        client.app.state.agent_pipeline = pipeline

        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "request-ws-progress",
                    "session_id": "test-ws-session",
                    "client_id": "test-ws-client",
                    "agent": "operator_guide",
                    "payload": {
                        "sub_agent": "operator_guide.recipe_explainer",
                        "question": "구리선은 어떻게 만들어?",
                    },
                }
            )

            messages = []
            for _ in range(10):
                msg = websocket.receive_json()
                messages.append(msg)
                if msg["type"] in ("agent.response", "agent.error"):
                    break

    progress_messages = [m for m in messages if m["type"] == "agent.progress"]
    assert [
        (m["payload"]["stage"], m["payload"]["message"]) for m in progress_messages
    ] == [
        ("rag_search", "관련 레시피 매뉴얼을 확인하는 중입니다..."),
        ("state_check", "필요한 재료와 장비를 살펴보는 중입니다..."),
        ("logic_format", "생산 흐름을 이해하기 쉽게 정리하는 중입니다..."),
    ]

    for progress_message in progress_messages:
        assert progress_message["request_id"] == "request-ws-progress"
        assert progress_message["session_id"] == "test-ws-session"
        assert progress_message["client_id"] == "test-ws-client"
        assert progress_message["agent"] == "operator_guide"
        assert "message" in progress_message["payload"]

    final_response = messages[-1]
    assert final_response["type"] == "agent.response"
    assert final_response["request_id"] == "request-ws-progress"
    assert "final_answer" in final_response["payload"]


def test_agent_websocket_progress_reflects_requested_agent() -> None:
    """progress 이벤트의 agent 라벨이 요청한 agent와 일치하는지 확인한다.

    초보자용 설명:
        `/agent-test` 콘솔에서 quest 프리셋을 보내면 진행 메시지도 quest 에이전트로
        표기돼야 합니다. 과거에는 항상 `operator_guide`로 고정 표기돼 quest 실행 중에도
        operator_guide처럼 보이는 혼동이 있었습니다.
    """

    class _ProgressPipeline:
        def run(
            self,
            message: dict[str, Any],
            on_progress: Callable[[str, str], None] | None = None,
        ) -> dict[str, Any]:
            if on_progress is not None:
                on_progress("build_prompt", "퀘스트를 생성하는 중입니다...")
            return {
                "type": "agent.response",
                "request_id": message.get("request_id"),
                "session_id": message.get("session_id"),
                "client_id": message.get("client_id"),
                "agent": "quest_generator",
                "payload": {"quests": []},
            }

    with TestClient(create_app()) as client:
        client.app.state.agent_pipeline = _ProgressPipeline()

        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "agent.request",
                    "request_id": "request-ws-quest",
                    "session_id": "test-ws-session",
                    "client_id": "test-ws-client",
                    "agent": "quest_generator",
                    "payload": {"sub_agent": "quest_generator.production_quest"},
                }
            )

            messages = []
            for _ in range(10):
                msg = websocket.receive_json()
                messages.append(msg)
                if msg["type"] in ("agent.response", "agent.error"):
                    break

    progress_messages = [m for m in messages if m["type"] == "agent.progress"]
    assert progress_messages
    for progress_message in progress_messages:
        assert progress_message["agent"] == "quest_generator"


def test_agent_websocket_returns_error_for_non_dict_json() -> None:
    with TestClient(create_app()) as client:
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json([1, 2, 3])
            response = websocket.receive_json()

    assert response["type"] == "agent.error"
    assert response["error"]["code"] == "INVALID_ENVELOPE"
