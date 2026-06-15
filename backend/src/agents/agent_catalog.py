"""Deterministic agent catalog used by routing prompts."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Protocol

from agents.base import AgentContext, AgentTool


@dataclass(frozen=True)
class AgentCapability:
    """Short routing support description for one top-level agent."""

    agent_id: str
    summary: str
    when_to_use: str


@dataclass(frozen=True)
class RoutingToolResult:
    """Prompt section returned by a deterministic routing support tool."""

    name: str
    section: str
    content: str


class RoutingSupportTool(AgentTool, Protocol):
    """Read-only tool interface for deterministic routing prompt support."""

    name: str

    def invoke(
        self,
        payload: dict[str, Any],
        context: AgentContext,
        args: dict[str, Any] | None = None,
    ) -> RoutingToolResult:
        """Return one prompt section for routing support."""


TOP_LEVEL_AGENT_CAPABILITIES = (
    AgentCapability(
        agent_id="process_optimizer",
        summary="공장 병목을 찾고 공정 개선 우선순위를 제안한다.",
        when_to_use="처리량, 설비 가동률, 병목, 개선 우선순위 요청에 사용한다.",
    ),
    AgentCapability(
        agent_id="operator_guide",
        summary="작업자 질문에 답하고 사용법, 레시피 설명, 문제 해결 요청을 처리한다.",
        when_to_use="설비 조작, 레시피 설명, 트러블슈팅, 현장 가이드 요청에 사용한다.",
    ),
    AgentCapability(
        agent_id="quest_generator",
        summary="튜토리얼, 생산, 탐험, 경제 퀘스트를 생성한다.",
        when_to_use="목표, 미션, 온보딩, 진행도, 게임플레이 퀘스트 요청에 사용한다.",
    ),
    AgentCapability(
        agent_id="material_generation",
        summary="설계 제약을 바탕으로 신소재 후보를 생성한다.",
        when_to_use="신소재 아이디어, 소재 목표, 속성, 희귀도, 생산 메모 요청에 사용한다.",
    ),
)


class AgentCatalogTool:
    """Return top-level agent capabilities for orchestrator routing."""

    name = "agent_catalog.get_capabilities"

    def invoke(
        self,
        payload: dict[str, Any],
        context: AgentContext,
        args: dict[str, Any] | None = None,
    ) -> RoutingToolResult:
        return RoutingToolResult(
            name=self.name,
            section="AGENT_CAPABILITIES",
            content=format_top_level_agent_capabilities(),
        )


def get_top_level_agent_capabilities() -> tuple[AgentCapability, ...]:
    """Return top-level agent capabilities in routing order."""

    return TOP_LEVEL_AGENT_CAPABILITIES


def create_default_routing_support_tools() -> tuple[RoutingSupportTool, ...]:
    """Create deterministic tools used to enrich routing prompts."""

    return (AgentCatalogTool(),)


def format_top_level_agent_capabilities() -> str:
    """Return a compact prompt section for top-level routing support."""

    return "\n".join(
        (
            f"- {capability.agent_id}: {capability.summary} "
            f"사용 기준: {capability.when_to_use}"
        )
        for capability in TOP_LEVEL_AGENT_CAPABILITIES
    )
