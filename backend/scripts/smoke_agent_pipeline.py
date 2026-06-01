"""Smoke runner for the agent WebSocket pipeline."""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import sys
import urllib.request
from collections.abc import Sequence
from dataclasses import dataclass
from typing import Any
from urllib.error import URLError
from urllib.parse import urljoin, urlsplit, urlunsplit

from websockets.asyncio.client import connect

DEFAULT_BASE_URL = "http://127.0.0.1:18000"
DEFAULT_WS_PATH = "/ws/agent"
EXTERNAL_PROVIDER_OPT_IN = "FACTORY_SMOKE_EXTERNAL_PROVIDER"


class SmokeError(AssertionError):
    """Raised when a smoke check fails."""


@dataclass(frozen=True)
class SmokeCase:
    """One smoke check against HTTP or WebSocket transport."""

    name: str
    message: dict[str, Any] | str | None
    expected_type: str | None = None
    expected_agent: str | None = None
    expected_sub_agent: str | None = None
    expected_quest_id: str | None = None
    expected_error_code: str | None = None
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

    if case.expected_quest_id is not None:
        quest_id = _quest_id(response)
        if quest_id != case.expected_quest_id:
            raise SmokeError(
                f"{case.name}: expected quest id {case.expected_quest_id}, "
                f"got {quest_id}"
            )


async def run_profile(profile: SmokeProfile, base_url: str, ws_path: str) -> int:
    """Run one smoke profile and return a process-style exit code."""

    if profile.requires_external_opt_in and os.environ.get(EXTERNAL_PROVIDER_OPT_IN) != "1":
        print(
            f"SKIP {profile.name}: set {EXTERNAL_PROVIDER_OPT_IN}=1 to run provider smoke"
        )
        return 0

    websocket_url = build_websocket_url(base_url, ws_path)
    for case in profile.cases:
        if case.transport == "http_health":
            response = check_health(base_url)
        else:
            response = await request_websocket_case(websocket_url, case)
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


async def request_websocket_case(
    websocket_url: str,
    case: SmokeCase,
) -> dict[str, Any]:
    """Send one WebSocket smoke message and parse the JSON response."""

    try:
        async with connect(websocket_url) as websocket:
            if isinstance(case.message, str):
                await websocket.send(case.message)
            else:
                await websocket.send(json.dumps(case.message))
            raw_response = await websocket.recv()
    except OSError as exc:
        raise SmokeError(f"{case.name}: websocket request failed: {exc}") from exc

    try:
        response = json.loads(raw_response)
    except json.JSONDecodeError as exc:
        raise SmokeError(
            f"{case.name}: response was not valid JSON: {raw_response}"
        ) from exc

    if not isinstance(response, dict):
        raise SmokeError(f"{case.name}: response must be an object, got {response}")
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
    profile_names = ("none", "local", "providers") if args.profile == "all" else (
        args.profile,
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
                "payload": {"machines": [{"id": "assembler-1", "utilization": 0.91}]},
            },
            expected_type="agent.response",
            expected_agent="process_optimizer",
            expected_sub_agent="process_optimizer",
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
                "payload": {
                    "message": "Create an iron ore mining quest.",
                    "sub_agent": "quest_generator.production_quest",
                    "game_state": {"quest_case": "mine_iron_ore_10"},
                },
            },
            expected_type="agent.response",
            expected_agent="quest_generator",
            expected_sub_agent="quest_generator.production_quest",
            expected_quest_id="quest_mine_iron_ore_10",
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


def _quest_id(response: dict[str, Any]) -> str | None:
    payload = response.get("payload")
    if not isinstance(payload, dict):
        return None

    quest = payload.get("quest")
    if not isinstance(quest, dict):
        return None

    quest_id = quest.get("id")
    return quest_id if isinstance(quest_id, str) else None


if __name__ == "__main__":
    raise SystemExit(main())
