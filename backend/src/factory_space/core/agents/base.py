"""Base agent contract."""

from __future__ import annotations

from typing import Protocol

from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import AgentRequest, AgentResponse


class BaseAgent(Protocol):
    """Protocol every domain agent must implement."""

    agent_id: str

    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        """Process one normalized agent request."""
        ...
