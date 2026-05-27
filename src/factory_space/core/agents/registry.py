"""Agent registry."""

from __future__ import annotations

from factory_space.agents.factory_optimization.agent import (
    create_agent as create_factory_optimization_agent,
)
from factory_space.agents.material_generation.agent import (
    create_agent as create_material_generation_agent,
)
from factory_space.agents.qa_chatbot.agent import (
    create_agent as create_qa_chatbot_agent,
)
from factory_space.agents.quest.agent import create_agent as create_quest_agent
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


def create_default_registry() -> AgentRegistry:
    """Create a registry with the initial project agents."""

    registry = AgentRegistry()
    registry.register(create_factory_optimization_agent())
    registry.register(create_qa_chatbot_agent())
    registry.register(create_quest_agent())
    registry.register(create_material_generation_agent())
    return registry
