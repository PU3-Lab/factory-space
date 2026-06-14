"""State definition for the material generation StateGraph."""

from __future__ import annotations

from typing import Any, TypedDict

from sqlalchemy.orm import Session

from agents.base import AgentContext
from agents.material_generation.schemas import (
    MaterialCreationRequest,
    MaterialCreationResponse,
    MaterialProposal,
)


class MaterialGraphState(TypedDict):
    """State dictionary passed between nodes in the material generation subgraph."""

    db: Session
    request: MaterialCreationRequest
    context: AgentContext
    normalized_inputs: list[dict[str, Any]]
    experiment_hash: str
    classification: str
    proposal: MaterialProposal | None
    attempt: int
    response: MaterialCreationResponse | None
    error: str | None
    similar_context: list[dict[str, Any]] | None
    is_new: bool | None
