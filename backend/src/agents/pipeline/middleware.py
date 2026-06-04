"""Shared pipeline middleware metadata helpers."""

from __future__ import annotations

import logging
from typing import Any

from agents.pipeline.state import AgentGraphState

LOGGER = logging.getLogger("agents.pipeline.runtime")


def append_middleware_log(
    state: AgentGraphState,
    node: str,
    event: str,
    details: dict[str, Any],
) -> AgentGraphState:
    """Append one middleware log entry using the graph state."""

    LOGGER.info(
        "%s %s",
        node,
        event,
        extra={"middleware_node": node, "middleware_event": event},
    )
    return {
        "middlewareLogs": [
            *state.get("middlewareLogs", []),
            {
                "node": node,
                "event": event,
                "details": details,
            },
        ]
    }


def build_current_model_metadata(state: AgentGraphState) -> dict[str, str] | None:
    """Build response metadata for the latest LLM slot used by the graph."""

    slot = state.get("llmSlot")
    provider = state.get("llmProvider")
    if not slot or not provider:
        return None

    metadata = {
        "slot": slot,
        "provider": provider,
    }
    model = state.get("llmModel")
    if model:
        metadata["model"] = model
    return metadata
