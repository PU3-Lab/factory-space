"""Smoke runner for the agent WebSocket pipeline."""

from __future__ import annotations

import argparse
import asyncio
import copy
import json
import os
import sys
import urllib.request
from collections.abc import Sequence
from dataclasses import dataclass
from typing import Any, Protocol
from urllib.error import URLError
from urllib.parse import urljoin, urlsplit, urlunsplit

from websockets.asyncio.client import connect

DEFAULT_BASE_URL = "http://127.0.0.1:18000"
DEFAULT_WS_PATH = "/ws/agent"
EXTERNAL_PROVIDER_OPT_IN = "FACTORY_SMOKE_EXTERNAL_PROVIDER"
FORBIDDEN_EXECUTION_COMMANDS = (
    "set_recipe",
    "set_machine_enabled",
    "connect_conveyor",
    "disconnect_conveyor",
    "move_machine",
    "place_machine",
    "remove_machine",
)
FORBIDDEN_EXECUTION_PAYLOAD_KEYS = ("command", "commands", "execution_commands")


class SmokeError(AssertionError):
    """Raised when a smoke check fails."""


class WebSocketReceiver(Protocol):
    """Minimal WebSocket interface used while waiting for terminal messages."""

    async def recv(self) -> str | bytes: ...


@dataclass(frozen=True)
class SmokeCase:
    """One smoke check against HTTP or WebSocket transport."""

    name: str
    message: dict[str, Any] | str | None
    expected_type: str | None = None
    expected_agent: str | None = None
    expected_sub_agent: str | None = None
    expected_quest_count: int | None = None
    expected_error_code: str | None = None
    expected_factory_revision: int | None = None
    expected_highlight_targets: tuple[str, ...] | None = None
    expected_status: str | None = None
    reject_execution_commands: bool = False
    transport: str = "websocket"


@dataclass(frozen=True)
class SmokeProfile:
    """Ordered smoke cases for one execution profile."""

    name: str
    cases: tuple[SmokeCase, ...]
    requires_external_opt_in: bool = False


def build_websocket_url(base_url: str, ws_path: str) -> str:
    """Build a WebSocket URL from the HTTP base URL and endpoint path."""

    parsed = urlsplit(base_url.rstrip("/"))
    scheme = "wss" if parsed.scheme == "https" else "ws"
    normalized_path = ws_path if ws_path.startswith("/") else f"/{ws_path}"
    path = f"{parsed.path.rstrip('/')}{normalized_path}"
    return urlunsplit((scheme, parsed.netloc, path, "", ""))


def build_profile(name: str) -> SmokeProfile:
    """Return the smoke profile for a known profile name."""

    if name == "none":
        return SmokeProfile(
            name="none",
            cases=(
                SmokeCase(name="health", message=None, transport="http_health"),
                SmokeCase(
                    name="agent_connection_manifest",
                    message=None,
                    transport="http_agent_connection_manifest",
                ),
                SmokeCase(
                    name="invalid_json",
                    message="{not-json",
                    expected_type="agent.error",
                    expected_error_code="INVALID_JSON",
                ),
                SmokeCase(
                    name="invalid_envelope",
                    message={
                        "type": "wrong.type",
                        "request_id": "request-smoke-invalid-envelope",
                        "payload": {},
                    },
                    expected_type="agent.error",
                    expected_error_code="INVALID_ENVELOPE",
                ),
                SmokeCase(
                    name="routing_unavailable",
                    message={
                        "type": "agent.request",
                        "request_id": "request-smoke-routing-unavailable",
                        "session_id": "smoke-session",
                        "client_id": "smoke-client",
                        "agent": "process_optimizer",
                        "payload": {"machines": [{"id": "assembler-1"}]},
                    },
                    expected_type="agent.error",
                    expected_agent="process_optimizer",
                    expected_error_code="ROUTING_UNAVAILABLE",
                ),
            ),
        )

    if name == "local":
        return SmokeProfile(name="local", cases=_agent_response_cases())

    if name == "providers":
        return SmokeProfile(
            name="providers",
            cases=_agent_response_cases(),
            requires_external_opt_in=True,
        )

    raise SmokeError(f"Unknown smoke profile: {name}")


def validate_case_response(case: SmokeCase, response: dict[str, Any]) -> None:
    """Validate one smoke response against the case contract."""

    if case.expected_type is not None and response.get("type") != case.expected_type:
        raise SmokeError(
            f"{case.name}: expected type {case.expected_type}, got {response.get('type')}"
        )

    if case.expected_agent is not None and response.get("agent") != case.expected_agent:
        raise SmokeError(
            f"{case.name}: expected agent {case.expected_agent}, got {response.get('agent')}"
        )

    if case.expected_error_code is not None:
        error = response.get("error")
        error_code = error.get("code") if isinstance(error, dict) else None
        if error_code != case.expected_error_code:
            raise SmokeError(
                f"{case.name}: expected error code {case.expected_error_code}, "
                f"got {error_code}"
            )

    if case.expected_sub_agent is not None:
        selected_leaf_agent = _selected_leaf_agent(response)
        if selected_leaf_agent != case.expected_sub_agent:
            raise SmokeError(
                f"{case.name}: expected selected leaf {case.expected_sub_agent}, "
                f"got {selected_leaf_agent}"
            )

    if case.expected_quest_count is not None:
        quest_count = _quest_count(response)
        if quest_count != case.expected_quest_count:
            raise SmokeError(
                f"{case.name}: expected {case.expected_quest_count} quests, "
                f"got {quest_count}"
            )

    if (
        case.expected_factory_revision is not None
        or case.expected_highlight_targets is not None
        or case.expected_status is not None
        or case.reject_execution_commands
    ):
        payload = response.get("payload")
        if not isinstance(payload, dict):
            raise SmokeError(f"{case.name}: expected response payload object")

        if (
            case.expected_status is not None
            and payload.get("status") != case.expected_status
        ):
            raise SmokeError(
                f"{case.name}: expected status {case.expected_status}, "
                f"got {payload.get('status')}"
            )

        if (
            case.expected_factory_revision is not None
            and payload.get("factoryRevision") != case.expected_factory_revision
        ):
            raise SmokeError(
                f"{case.name}: expected factoryRevision "
                f"{case.expected_factory_revision}, got {payload.get('factoryRevision')}"
            )

        if case.expected_highlight_targets is not None:
            ui_hints = payload.get("ui_hints")
            highlight_targets = (
                ui_hints.get("highlight_targets") if isinstance(ui_hints, dict) else None
            )
            if not isinstance(highlight_targets, list) or not all(
                target in highlight_targets for target in case.expected_highlight_targets
            ):
                raise SmokeError(
                    f"{case.name}: expected highlight targets "
                    f"{list(case.expected_highlight_targets)}, got {highlight_targets}"
                )

        if case.reject_execution_commands:
            _validate_no_execution_commands(case.name, payload)


async def run_profile(profile: SmokeProfile, base_url: str, ws_path: str) -> int:
    """Run one smoke profile and return a process-style exit code."""

    if (
        profile.requires_external_opt_in
        and os.environ.get(EXTERNAL_PROVIDER_OPT_IN) != "1"
    ):
        print(
            f"SKIP {profile.name}: set {EXTERNAL_PROVIDER_OPT_IN}=1 to run provider smoke"
        )
        return 0

    websocket_url = build_websocket_url(base_url, ws_path)
    last_plan_id = None

    cases = list(profile.cases)
    if profile.name in ("local", "providers"):
        cases.extend(_v2_process_optimizer_cases())

    for case in cases:
        msg = copy.deepcopy(case.message)
        if isinstance(msg, dict) and last_plan_id:
            def replace_placeholder(d):
                if isinstance(d, dict):
                    for k, v in d.items():
                        if v == "{PLAN_ID}":
                            d[k] = last_plan_id
                        else:
                            replace_placeholder(v)
                elif isinstance(d, list):
                    for i, v in enumerate(d):
                        if v == "{PLAN_ID}":
                            d[i] = last_plan_id
                        else:
                            replace_placeholder(v)
            replace_placeholder(msg)

        if case.transport == "http_health":
            response = check_health(base_url)
        elif case.transport == "http_agent_connection_manifest":
            response = check_agent_connection_manifest(base_url)
        else:
            response = await request_websocket_case(websocket_url, case, msg)
            if isinstance(response, dict):
                payload = response.get("payload")
                if isinstance(payload, dict) and payload.get("plan_id"):
                    last_plan_id = payload.get("plan_id")
        validate_case_response(case, response)
        print(f"PASS {profile.name}/{case.name}")

    return 0


def check_health(base_url: str) -> dict[str, Any]:
    """Call the health endpoint and return a response-shaped dict."""

    health_url = urljoin(f"{base_url.rstrip('/')}/", "health")
    try:
        with urllib.request.urlopen(health_url, timeout=5) as response:
            body = json.loads(response.read().decode("utf-8"))
    except (OSError, json.JSONDecodeError, URLError) as exc:
        raise SmokeError(f"health: failed to call {health_url}: {exc}") from exc

    if body != {"status": "ok"}:
        raise SmokeError(f"health: expected {{'status': 'ok'}}, got {body}")
    return {"type": None}


def check_agent_connection_manifest(base_url: str) -> dict[str, Any]:
    """Call the agent connection manifest and validate the Unreal contract."""

    manifest_url = urljoin(f"{base_url.rstrip('/')}/", "api/v1/agent-connection")
    try:
        with urllib.request.urlopen(manifest_url, timeout=5) as response:
            body = json.loads(response.read().decode("utf-8"))
    except (OSError, json.JSONDecodeError, URLError) as exc:
        raise SmokeError(f"manifest: failed to call {manifest_url}: {exc}") from exc

    expected_fields = {
        "status": "ok",
        "websocket_path": "/ws/agent",
        "request_type": "agent.request",
        "response_types": ["agent.response", "agent.error"],
    }
    for field, expected_value in expected_fields.items():
        if body.get(field) != expected_value:
            raise SmokeError(
                f"manifest: expected {field} {expected_value}, got {body.get(field)}"
            )

    top_level_agents = body.get("top_level_agents")
    if not isinstance(top_level_agents, list) or not top_level_agents:
        raise SmokeError("manifest: expected non-empty top_level_agents")
    return {"type": None}


async def request_websocket_case(
    websocket_url: str,
    case: SmokeCase,
    message: dict[str, Any] | str | None = None,
) -> dict[str, Any]:
    """Send one WebSocket smoke message and parse the JSON response."""

    outbound_message = case.message if message is None else message
    try:
        async with connect(websocket_url) as websocket:
            if isinstance(outbound_message, str):
                await websocket.send(outbound_message)
            else:
                await websocket.send(json.dumps(outbound_message))
            return await receive_terminal_response(websocket)
    except OSError as exc:
        raise SmokeError(f"{case.name}: websocket request failed: {exc}") from exc


async def receive_terminal_response(websocket: WebSocketReceiver) -> dict[str, Any]:
    """Skip progress events and return the final agent response or error."""

    while True:
        raw_response = await websocket.recv()
        try:
            response = json.loads(raw_response)
        except json.JSONDecodeError as exc:
            raise SmokeError(
                f"websocket response was not valid JSON: {raw_response}"
            ) from exc

        if not isinstance(response, dict):
            raise SmokeError(f"websocket response must be an object, got {response}")

        if response.get("type") in {"agent.response", "agent.error"}:
            return response


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """Parse command-line arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "profile",
        choices=("none", "local", "providers", "all"),
        help="Smoke profile to run.",
    )
    parser.add_argument(
        "--base-url",
        default=os.environ.get("FACTORY_SMOKE_BASE_URL", DEFAULT_BASE_URL),
        help="Backend HTTP base URL. Defaults to %(default)s.",
    )
    parser.add_argument(
        "--ws-path",
        default=os.environ.get("FACTORY_SMOKE_WS_PATH", DEFAULT_WS_PATH),
        help="Agent WebSocket path. Defaults to %(default)s.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    """Run the smoke command."""

    args = parse_args(sys.argv[1:] if argv is None else argv)
    profile_names = (
        ("none", "local", "providers") if args.profile == "all" else (args.profile,)
    )
    try:
        for profile_name in profile_names:
            exit_code = asyncio.run(
                run_profile(build_profile(profile_name), args.base_url, args.ws_path)
            )
            if exit_code != 0:
                return exit_code
    except SmokeError as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1
    return 0


def _agent_response_cases() -> tuple[SmokeCase, ...]:
    return (
        SmokeCase(
            name="process_optimizer",
            message={
                "type": "agent.request",
                "request_id": "request-smoke-process",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "analyze",
                    "goal": "balance",
                    "factoryRevision": 1,
                    "factory_state": {
                        "machines": [
                            {
                                "id": "smelter_1",
                                "type": "smelter",
                                "status": "operating",
                                "operating_rate": 0.5,
                                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                            }
                        ],
                        "conveyors": [],
                        "power_grid": {"produced": 100, "consumed": 50}
                    },
                },
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_sub_agent="process_optimizer",
            expected_factory_revision=1,
            expected_highlight_targets=("smelter_1",),
            expected_status="preview",
            reject_execution_commands=True,
        ),
        SmokeCase(
            name="operator_guide",
            message={
                "type": "agent.request",
                "request_id": "request-smoke-operator-guide",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "operator_guide",
                "payload": {
                    "question": "How do I use this machine panel?",
                    "sub_agent": "operator_guide.machine_help",
                },
            },
            expected_type="agent.response",
            expected_agent="operator_guide",
            expected_sub_agent="operator_guide.machine_help",
        ),
        SmokeCase(
            name="quest_generator",
            message={
                "type": "agent.request",
                "request_id": "request-smoke-quest",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "quest_generator",
            },
            expected_type="agent.response",
            expected_agent="quest_generator",
            expected_sub_agent="quest_generator.production_quest",
            expected_quest_count=5,
        ),
        SmokeCase(
            name="new_material_generator",
            message={
                "type": "agent.request",
                "request_id": "request-smoke-material",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "new_material_generator",
                "payload": {"theme": "conductive alloy", "rarity": "rare"},
            },
            expected_type="agent.response",
            expected_agent="new_material_generator",
            expected_sub_agent="new_material_generator",
        ),
    )


def _v2_process_optimizer_cases() -> tuple[SmokeCase, ...]:
    return (
        # 1. 주기 상태 업데이트 -> session memory 갱신, 공장 변경 없음
        SmokeCase(
            name="process_optimizer_state_update",
            message={
                "type": "agent.request",
                "request_id": "req-smoke-v2-state-update",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "state_update",
                    "goal": "balance",
                    "factoryRevision": 1,
                    "factory_state": {
                        "machines": [],
                        "conveyors": [],
                        "power_grid": {"produced": 100, "consumed": 50},
                    },
                },
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_factory_revision=1,
            expected_status="success",
            reject_execution_commands=True,
        ),
        # 2. approval 없는 apply -> approval_required
        SmokeCase(
            name="process_optimizer_apply_no_app",
            message={
                "type": "agent.request",
                "request_id": "req-smoke-v2-apply-no-app",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "apply",
                    "plan_id": "{PLAN_ID}",
                    "factoryRevision": 1,
                    "approval": False
                }
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_status="approval_required",
        ),
        # 3. 정상 apply -> command payload 반환
        SmokeCase(
            name="process_optimizer_apply_success",
            message={
                "type": "agent.request",
                "request_id": "req-smoke-v2-apply-success",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "apply",
                    "plan_id": "{PLAN_ID}",
                    "factoryRevision": 1,
                    "approval": True,
                    "approved_change_ids": ["suggest_input_smelter_1"],
                    "factory_state": {
                        "machines": [
                            {
                                "id": "smelter_1",
                                "type": "smelter",
                                "status": "operating",
                                "operating_rate": 0.5,
                                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                            }
                        ],
                        "conveyors": [],
                        "power_grid": {"produced": 100, "consumed": 50}
                    }
                }
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_status="execute_ready",
        ),
        # 4. factoryRevision 변경 -> revision_conflict
        SmokeCase(
            name="process_optimizer_apply_conflict",
            message={
                "type": "agent.request",
                "request_id": "req-smoke-v2-apply-conflict",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "apply",
                    "plan_id": "{PLAN_ID}",
                    "factoryRevision": 2,
                    "approval": True
                }
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_status="revision_conflict",
        ),
        # 5. undo 충돌 -> undo_conflict
        SmokeCase(
            name="process_optimizer_undo_conflict",
            message={
                "type": "agent.request",
                "request_id": "req-smoke-v2-undo-conflict",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "undo",
                    "plan_id": "{PLAN_ID}",
                    "factory_state": {
                        "machines": [
                            {
                                "id": "smelter_1",
                                "type": "smelter",
                                "status": "operating",
                                "recipe_id": "copper_ingot"
                            }
                        ],
                        "conveyors": []
                    }
                }
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_status="undo_conflict",
        ),
        # 6. measure 준비 전 -> measurement_not_ready (주기 1회)
        SmokeCase(
            name="process_optimizer_measure_not_ready",
            message={
                "type": "agent.request",
                "request_id": "req-smoke-v2-measure-not-ready",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "measure",
                    "plan_id": "{PLAN_ID}",
                    "production_cycles": 1,
                    "factory_state": {
                        "machines": [{"id": "smelter_1", "type": "smelter", "status": "operating"}],
                        "conveyors": []
                    }
                }
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_status="measurement_not_ready",
        ),
        # 7. measure 완료 -> measurement summary 반환
        SmokeCase(
            name="process_optimizer_measure_ready",
            message={
                "type": "agent.request",
                "request_id": "req-smoke-v2-measure-ready",
                "session_id": "smoke-session",
                "client_id": "smoke-client",
                "agent": "process_optimizer",
                "payload": {
                    "operation": "measure",
                    "plan_id": "{PLAN_ID}",
                    "production_cycles": 5,
                    "current_time": "2030-01-01T00:00:00Z",
                    "factory_state": {
                        "machines": [
                            {
                                "id": "smelter_1",
                                "type": "smelter",
                                "status": "operating",
                                "operating_rate": 1.0,
                                "inputs": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 10.0}]
                            }
                        ],
                        "conveyors": []
                    }
                }
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_status="measurement_ready",
        ),
    )


def _selected_leaf_agent(response: dict[str, Any]) -> str | None:
    payload = response.get("payload")
    if not isinstance(payload, dict):
        return None

    metadata = payload.get("metadata")
    if isinstance(metadata, dict):
        selected_leaf_agent = metadata.get("selectedLeafAgent")
        if isinstance(selected_leaf_agent, str):
            return selected_leaf_agent

    sub_agent = response.get("sub_agent")
    return sub_agent if isinstance(sub_agent, str) else None


def _quest_count(response: dict[str, Any]) -> int | None:
    payload = response.get("payload")
    if not isinstance(payload, dict):
        return None

    quests = payload.get("quests")
    if not isinstance(quests, list):
        return None
    return len(quests)


def _validate_no_execution_commands(name: str, payload: dict[str, Any]) -> None:
    """Reject raw Unreal execution commands in a suggestion-only smoke response."""

    payload_json = json.dumps(payload, ensure_ascii=False).lower()
    for command in FORBIDDEN_EXECUTION_COMMANDS:
        if command in payload_json:
            raise SmokeError(f"{name}: forbidden execution command {command}")

    for key in FORBIDDEN_EXECUTION_PAYLOAD_KEYS:
        if f'"{key}"' in payload_json:
            raise SmokeError(f"{name}: forbidden execution command payload key {key}")


if __name__ == "__main__":
    raise SystemExit(main())
