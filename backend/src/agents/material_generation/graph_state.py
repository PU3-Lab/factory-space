"""재료 생성 StateGraph를 위한 상태(State) 정의입니다."""

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
    """재료 생성 서브그래프의 노드들 사이에서 전달되는 상태 딕셔너리입니다."""

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
