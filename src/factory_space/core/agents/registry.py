"""Agent registry."""

from __future__ import annotations

from factory_space.core.agents.base import BaseAgent


class AgentRegistry:
    """In-memory registry for domain agents."""

    def __init__(self) -> None:
        self._agents: dict[str, BaseAgent] = {}

    def register(self, agent: BaseAgent) -> None:
        """Register an agent by its `agent_id`."""

        self._agents[agent.agent_id] = agent

    def get(self, agent_id: str) -> BaseAgent:
        """Return an agent or raise a clear lookup error."""

        try:
            return self._agents[agent_id]
        except KeyError as error:
            raise UnknownAgentError(agent_id) from error

    def contains(self, agent_id: str) -> bool:
        """Return whether an agent is registered."""

        return agent_id in self._agents

    def list_agent_ids(self) -> list[str]:
        """Return registered agent ids."""

        return sorted(self._agents)


class UnknownAgentError(LookupError):
    """Raised when a request references an unregistered agent."""

    def __init__(self, agent_id: str) -> None:
        super().__init__(f"Unknown agent: {agent_id}")
        self.agent_id = agent_id
