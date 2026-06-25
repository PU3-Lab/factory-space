from __future__ import annotations

import asyncio

import pytest

from scripts import smoke_agent_pipeline as smoke


def test_none_profile_contains_no_external_api_cases() -> None:
    profile = smoke.build_profile("none")

    assert profile.name == "none"
    assert [case.name for case in profile.cases] == [
        "health",
        "agent_connection_manifest",
        "invalid_json",
        "invalid_envelope",
        "routing_unavailable",
    ]
    assert profile.requires_external_opt_in is False


def test_local_profile_exercises_all_agent_paths() -> None:
    profile = smoke.build_profile("local")

    assert profile.name == "local"
    assert [case.expected_agent for case in profile.cases] == [
        "process_optimizer",
        "operator_guide",
        "quest_generator",
        "new_material_generator",
    ]
    assert [case.expected_sub_agent for case in profile.cases] == [
        "process_optimizer",
        "operator_guide.machine_help",
        "quest_generator.production_quest",
        "new_material_generator",
    ]
    quest_case = profile.cases[2]
    assert quest_case.expected_quest_count == 5
    assert isinstance(quest_case.message, dict)
    assert "payload" not in quest_case.message


def test_local_process_optimizer_case_declares_response_contract() -> None:
    """Process Optimizer smoke 케이스가 Unreal 응답 핵심 계약을 선언하는지 검증합니다."""
    process_case = smoke.build_profile("local").cases[0]

    assert process_case.expected_factory_revision == 1
    assert process_case.expected_highlight_targets == ("smelter_1",)
    assert process_case.reject_execution_commands is True


def test_response_validation_rejects_execution_command_in_optimizer_suggestion() -> None:
    """제안 응답에 자동 실행 명령이 섞이면 smoke 검증이 실패해야 합니다."""
    case = smoke.SmokeCase(
        name="process optimizer",
        message={"type": "agent.request"},
        expected_type="agent.response",
        expected_agent="process_optimizer",
        expected_factory_revision=1,
        expected_highlight_targets=("smelter_1",),
        reject_execution_commands=True,
    )

    with pytest.raises(smoke.SmokeError, match="forbidden execution command"):
        smoke.validate_case_response(
            case,
            {
                "type": "agent.response",
                "agent": "process_optimizer",
                "payload": {
                    "factoryRevision": 1,
                    "suggestions": [
                        {"recommended_action": "set_recipe smelter_1 iron_plate"}
                    ],
                    "ui_hints": {"highlight_targets": ["smelter_1"]},
                },
            },
        )


def test_response_validation_rejects_wrong_expected_status() -> None:
    """Smoke validation must fail when a v2 case returns the wrong payload status."""
    case = smoke.SmokeCase(
        name="process optimizer measure",
        message={"type": "agent.request"},
        expected_type="agent.response",
        expected_agent="process_optimizer",
        expected_status="measurement_ready",
    )

    with pytest.raises(smoke.SmokeError, match="expected status measurement_ready"):
        smoke.validate_case_response(
            case,
            {
                "type": "agent.response",
                "agent": "process_optimizer",
                "payload": {"status": "measurement_not_ready"},
            },
        )


def test_provider_profile_requires_explicit_opt_in() -> None:
    profile = smoke.build_profile("providers")

    assert profile.name == "providers"
    assert profile.requires_external_opt_in is True
    assert [case.expected_agent for case in profile.cases] == [
        "process_optimizer",
        "operator_guide",
        "quest_generator",
        "new_material_generator",
    ]


def test_provider_profile_skips_without_explicit_opt_in(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv(smoke.EXTERNAL_PROVIDER_OPT_IN, raising=False)

    exit_code = asyncio.run(
        smoke.run_profile(
            smoke.build_profile("providers"),
            smoke.DEFAULT_BASE_URL,
            smoke.DEFAULT_WS_PATH,
        )
    )

    assert exit_code == 0


def test_local_run_profile_executes_v2_cases_with_replaced_plan_id(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The local runner appends v2 cases and sends the replaced plan_id payload."""
    sent_messages: list[dict[str, object]] = []

    async def fake_request_websocket_case(
        websocket_url: str,
        case: smoke.SmokeCase,
        message: dict[str, object] | str | None = None,
    ) -> dict[str, object]:
        assert websocket_url == "ws://127.0.0.1:18000/ws/agent"
        assert isinstance(message, dict)
        sent_messages.append(message)

        payload = {
            "status": case.expected_status or "preview",
            "plan_id": "plan-run-profile",
            "factoryRevision": case.expected_factory_revision,
            "ui_hints": {"highlight_targets": ["smelter_1"]},
            "quests": [{} for _ in range(case.expected_quest_count or 0)],
            "metadata": {"selectedLeafAgent": case.expected_sub_agent},
        }
        return {
            "type": case.expected_type or "agent.response",
            "agent": case.expected_agent,
            "payload": payload,
        }

    monkeypatch.setattr(
        smoke,
        "request_websocket_case",
        fake_request_websocket_case,
    )

    exit_code = asyncio.run(
        smoke.run_profile(
            smoke.build_profile("local"),
            smoke.DEFAULT_BASE_URL,
            smoke.DEFAULT_WS_PATH,
        )
    )

    assert exit_code == 0
    operations = [
        message["payload"]["operation"]
        for message in sent_messages
        if isinstance(message.get("payload"), dict)
        and message["payload"].get("operation") is not None
    ]
    assert operations == [
        "analyze",
        "state_update",
        "apply",
        "apply",
        "apply",
        "undo",
        "measure",
        "measure",
    ]
    assert all("{PLAN_ID}" not in str(message) for message in sent_messages)
    assert any(
        message.get("payload", {}).get("plan_id") == "plan-run-profile"
        for message in sent_messages
        if isinstance(message.get("payload"), dict)
    )


def test_websocket_url_is_derived_from_http_base_url() -> None:
    assert (
        smoke.build_websocket_url("http://127.0.0.1:18000", "/ws/agent")
        == "ws://127.0.0.1:18000/ws/agent"
    )
    assert (
        smoke.build_websocket_url("https://example.test/api", "ws/agent")
        == "wss://example.test/api/ws/agent"
    )


def test_agent_connection_manifest_check_validates_contract(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    requested_urls: list[str] = []

    class FakeResponse:
        def __enter__(self) -> FakeResponse:
            return self

        def __exit__(self, *args: object) -> None:
            return None

        def read(self) -> bytes:
            return (
                b'{"status":"ok","websocket_path":"/ws/agent",'
                b'"request_type":"agent.request",'
                b'"response_types":["agent.response","agent.error"],'
                b'"top_level_agents":["process_optimizer"]}'
            )

    def fake_urlopen(url: str, timeout: int) -> FakeResponse:
        requested_urls.append(url)
        assert timeout == 5
        return FakeResponse()

    monkeypatch.setattr(smoke.urllib.request, "urlopen", fake_urlopen)

    response = smoke.check_agent_connection_manifest("http://127.0.0.1:18000")

    assert requested_urls == ["http://127.0.0.1:18000/api/v1/agent-connection"]
    assert response == {"type": None}


def test_response_validation_accepts_expected_agent_response() -> None:
    case = smoke.SmokeCase(
        name="operator guide",
        message={
            "type": "agent.request",
            "request_id": "request-smoke",
            "agent": "operator_guide",
            "payload": {"question": "How do I use this panel?"},
        },
        expected_type="agent.response",
        expected_agent="operator_guide",
        expected_sub_agent="operator_guide.machine_help",
    )

    smoke.validate_case_response(
        case,
        {
            "type": "agent.response",
            "agent": "operator_guide",
            "payload": {
                "metadata": {
                    "selectedLeafAgent": "operator_guide.machine_help",
                }
            },
        },
    )


def test_response_validation_rejects_wrong_error_code() -> None:
    case = smoke.SmokeCase(
        name="routing unavailable",
        message={
            "type": "agent.request",
            "request_id": "request-smoke",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        },
        expected_type="agent.error",
        expected_agent="process_optimizer",
        expected_error_code="ROUTING_UNAVAILABLE",
    )

    try:
        smoke.validate_case_response(
            case,
            {
                "type": "agent.error",
                "agent": "process_optimizer",
                "error": {"code": "INVALID_PAYLOAD"},
            },
        )
    except smoke.SmokeError as exc:
        assert "ROUTING_UNAVAILABLE" in str(exc)
    else:
        raise AssertionError("Expected SmokeError")


def test_response_validation_rejects_wrong_quest_count() -> None:
    case = smoke.SmokeCase(
        name="quest",
        message={
            "type": "agent.request",
            "request_id": "request-smoke",
            "agent": "quest_generator",
        },
        expected_type="agent.response",
        expected_agent="quest_generator",
        expected_quest_count=5,
    )

    try:
        smoke.validate_case_response(
            case,
            {
                "type": "agent.response",
                "agent": "quest_generator",
                "payload": {"quests": [{"id": 1}]},
            },
        )
    except smoke.SmokeError as exc:
        assert "expected 5 quests" in str(exc)
    else:
        raise AssertionError("Expected SmokeError")


def test_terminal_response_reader_skips_progress_messages() -> None:
    """Smoke Runner가 진행 메시지 뒤의 최종 응답을 받아야 합니다."""

    class FakeWebSocket:
        def __init__(self) -> None:
            self.messages = iter(
                [
                    '{"type":"agent.progress","payload":{"stage":"rag_search"}}',
                    '{"type":"agent.response","agent":"operator_guide","payload":{}}',
                ]
            )

        async def recv(self) -> str:
            return next(self.messages)

    response = asyncio.run(smoke.receive_terminal_response(FakeWebSocket()))

    assert response["type"] == "agent.response"
    assert response["agent"] == "operator_guide"
