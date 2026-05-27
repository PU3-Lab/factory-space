"""Agent orchestration."""

from __future__ import annotations

from factory_space.core.agents.registry import AgentRegistry
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import AgentRequest, AgentResponse


class AgentOrchestrator:
    """Coordinates request execution for registered agents."""

    def __init__(self, registry: AgentRegistry) -> None:
        self._registry = registry

    async def process(self, request: AgentRequest) -> AgentResponse:
        """Build context and dispatch the request to the target agent."""

        agent = self._registry.get(request.agent)
        context = AgentContext(
            session_id=request.session_id,
            client_id=request.client_id,
            request_id=request.request_id,
        )
        return await agent.process(request, context)
