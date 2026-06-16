"""Agent message envelope models."""

from __future__ import annotations

from typing import Any, Literal
from uuid import uuid4

from pydantic import BaseModel, ConfigDict, Field


class AgentRequestEnvelope(BaseModel):
    """Public request envelope accepted by the agent pipeline."""

    model_config = ConfigDict(extra="allow")

    type: Literal["agent.request"] = "agent.request"
    request_id: str = Field(default_factory=lambda: str(uuid4()))
    session_id: str | None = None
    client_id: str | None = None
    agent: str | None = None
    payload: dict[str, Any] = Field(default_factory=dict)
    context: dict[str, Any] = Field(default_factory=dict)


class AgentResponseEnvelope(BaseModel):
    """Public response envelope returned by the agent pipeline."""

    type: Literal["agent.response"] = "agent.response"
    request_id: str
    session_id: str | None = None
    client_id: str | None = None
    agent: str
    payload: dict[str, Any]
    streams: list[dict[str, Any]] = Field(default_factory=list)


class AgentProgressEnvelope(BaseModel):
    """에이전트 파이프라인의 실행 과정 중 진행 정보를 알리기 위해 전송되는 메시지 봉투(Envelope) 모델.

    초보자용 설명:
        LLM이 답변을 마칠 때까지 수 초가 걸리기 때문에, 그 사이에 플레이어에게
        현재 어떤 작업(예: 장비 확인, RAG 검색 등)이 진행 중인지 실시간으로 알려주기 위한 용도입니다.
    """

    type: Literal["agent.progress"] = "agent.progress"
    request_id: str
    session_id: str | None = None
    client_id: str | None = None
    agent: str = "operator_guide"
    payload: dict[str, Any]


class AgentErrorEnvelope(BaseModel):
    """Public error envelope returned by the agent pipeline."""

    type: Literal["agent.error"] = "agent.error"
    request_id: str | None = None
    session_id: str | None = None
    client_id: str | None = None
    agent: str | None = None
    error: dict[str, Any]
