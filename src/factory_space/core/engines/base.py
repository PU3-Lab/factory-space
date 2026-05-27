"""Base reasoning engine contract."""

from __future__ import annotations

from typing import Protocol

from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import AgentRequest, AgentResponse


class ReasoningEngine(Protocol):
    """Optional protocol for agent-owned reasoning engines."""

    async def run(
        self,
        request: AgentRequest,
        context: AgentContext,
        **resources: object,
    ) -> AgentResponse:
        """Run the engine for one agent request."""
        ...
