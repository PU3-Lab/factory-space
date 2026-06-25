"""Process Optimizer v2 request guards and state handoff helpers."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext
from agents.process_optimizer.schemas import FactoryState, ProcessOptimizerPayload
from agents.process_optimizer.session_memory import process_optimizer_memory
from protocol.errors import build_error_payload


def validate_process_optimizer_payload(payload: dict[str, Any]) -> dict[str, Any] | None:
    """Validate the public process_optimizer request before the v2 graph runs."""
    explicit_sub_agent = payload.get("sub_agent")
    if explicit_sub_agent is not None and explicit_sub_agent != "process_optimizer":
        return build_error_payload(
            "INVALID_SUB_AGENT",
            "Explicit sub_agent is not valid for process_optimizer.",
            details={"sub_agent": explicit_sub_agent},
        )

    try:
        ProcessOptimizerPayload.model_validate(payload)

        revision = payload.get("factoryRevision")
        if revision is not None:
            if not isinstance(revision, int) or isinstance(revision, bool):
                raise ValueError("factoryRevision must be an integer")
            if revision < 0:
                raise ValueError("factoryRevision cannot be negative")

        factory_state = payload.get("factory_state")
        if factory_state is not None:
            if not isinstance(factory_state, dict):
                raise ValueError("factory_state must be a dictionary")
            FactoryState.model_validate(factory_state)

    except Exception as exc:
        return build_error_payload(
            "INVALID_REQUEST_PAYLOAD",
            f"Request payload validation failed: {exc}",
        )

    return None


def build_state_update_response(
    context: AgentContext,
    payload: dict[str, Any],
) -> dict[str, Any]:
    """Store the latest periodic factory state and return a lightweight response."""
    factory_state = payload.get("factory_state")
    revision = payload.get("factoryRevision")
    if revision is None:
        revision = 0

    process_optimizer_memory.update(context.session_id, factory_state, revision)

    goal = payload.get("goal") or "balance"
    response_payload = {
        "status": "success",
        "factoryRevision": revision,
        "goal": goal,
    }
    return {
        "responsePayload": response_payload,
        "responseMetadata": {"memory": "updated"},
    }


def build_graph_payload_with_memory(
    context: AgentContext,
    payload: dict[str, Any],
) -> dict[str, Any]:
    """Fill missing factory state or revision from the latest state_update memory."""
    graph_payload = dict(payload)

    if "factory_state" not in graph_payload:
        remembered_state = process_optimizer_memory.get_state(context.session_id)
        if remembered_state:
            graph_payload["factory_state"] = remembered_state

    if graph_payload.get("factoryRevision") is None:
        remembered_revision = process_optimizer_memory.get_revision(context.session_id)
        if remembered_revision:
            graph_payload["factoryRevision"] = remembered_revision

    return graph_payload
