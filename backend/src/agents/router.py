"""Agent registry and router."""

from __future__ import annotations

from agents.base import Agent
from agents.material_generation.agent import MaterialCreationAgent
from agents.new_material_generator import NewMaterialGeneratorAgent
from agents.operator_guide.machine_help import MachineHelpAgent
from agents.operator_guide.recipe_explainer import RecipeExplainerAgent
from agents.operator_guide.troubleshooter import TroubleshooterAgent
from agents.quest_generator.economy_quest import EconomyQuestAgent
from agents.quest_generator.production_quest import ProductionQuestAgent


class UnknownAgentError(ValueError):
    """Raised when an agent id is not registered."""


class AgentRouter:
    """Map agent ids to concrete agent implementations."""

    def __init__(self) -> None:
        self._agents: dict[str, Agent] = {}

    def register(self, agent: Agent) -> None:
        """Register an agent implementation."""

        self._agents[agent.agent_id] = agent

    def get(self, agent_id: str) -> Agent:
        """Return a registered agent."""

        try:
            return self._agents[agent_id]
        except KeyError as exc:
            raise UnknownAgentError(f"Unknown agent: {agent_id}") from exc

    def has(self, agent_id: str) -> bool:
        """Return whether an agent id is registered."""

        return agent_id in self._agents

    def list_agent_ids(self) -> list[str]:
        """Return registered agent ids."""

        return sorted(self._agents)


def create_default_agent_router() -> AgentRouter:
    """Create the default router for top-level and leaf agents."""

    router = AgentRouter()
    for agent in (
        MaterialCreationAgent(),
        NewMaterialGeneratorAgent(),
        RecipeExplainerAgent(),
        MachineHelpAgent(),
        TroubleshooterAgent(),
        ProductionQuestAgent(),
        EconomyQuestAgent(),
    ):
        router.register(agent)
    return router
