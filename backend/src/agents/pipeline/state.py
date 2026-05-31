"""Shared state types for the agent pipeline."""

from __future__ import annotations

from typing import Any, Literal, TypedDict

from agents.base import AgentContext
from protocol.messages import AgentRequestEnvelope

TopRoute = Literal[
    "process_optimizer",
    "manual_qa",
    "quest_generator",
    "new_material_generator",
    "error",
]


class AgentGraphState(TypedDict, total=False):
    """Shared LangGraph state for one agent request."""

    envelope: AgentRequestEnvelope
    context: AgentContext
    selectedAgent: str
    selectedLeafAgent: str
    typedPayload: dict[str, Any]
    cacheKey: str
    cachedPayload: dict[str, Any]
    cachedMetadata: dict[str, Any]
    prompt: str
    routingPrompt: str
    routingRaw: str | None
    llmRaw: str | None
    llmSlot: str
    llmProvider: str
    llmModel: str
    fallbackReason: str
    responsePayload: dict[str, Any]
    responseMetadata: dict[str, Any]
    streams: list[dict[str, Any]]
    error: dict[str, Any]
    responseEnvelope: dict[str, Any]
