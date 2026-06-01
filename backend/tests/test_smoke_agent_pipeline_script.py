from __future__ import annotations

import asyncio

import pytest

from scripts import smoke_agent_pipeline as smoke


def test_none_profile_contains_no_external_api_cases() -> None:
    profile = smoke.build_profile("none")

    assert profile.name == "none"
    assert [case.name for case in profile.cases] == [
        "health",
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


def test_websocket_url_is_derived_from_http_base_url() -> None:
    assert (
        smoke.build_websocket_url("http://127.0.0.1:8000", "/ws/agent")
        == "ws://127.0.0.1:8000/ws/agent"
    )
    assert (
        smoke.build_websocket_url("https://example.test/api", "ws/agent")
        == "wss://example.test/api/ws/agent"
    )


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
