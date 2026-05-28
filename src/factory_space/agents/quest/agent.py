"""Quest agent entrypoint."""

from __future__ import annotations

from factory_space.agents.quest.schemas import QuestGenerationResult
from factory_space.agents.quest.service import QuestAgentService
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import (
    AgentRequest,
    AgentResponse,
    AgentResponsePayload,
)


class QuestAgent:
    """Quest agent that creates validated quests from game state."""

    agent_id = "quest"

    def __init__(self, service: QuestAgentService | None = None) -> None:
        self._service = service or QuestAgentService()

    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        """Return a quest response that keeps the Unreal-facing contract stable."""

        quest_json = self._service.generate_quest_json(request.payload)
        result = QuestGenerationResult.model_validate_json(quest_json)

        return AgentResponse(
            session_id=context.session_id,
            request_id=context.request_id,
            client_id=context.client_id,
            agent=self.agent_id,
            payload=AgentResponsePayload(
                text=result.text,
                actions=result.actions,
                metadata={
                    "status": "ok",
                    "quest": result.quest.model_dump(),
                    "quest_json": quest_json,
                    **result.metadata,
                },
            ),
        )


def create_agent() -> QuestAgent:
    """Create the default quest agent."""

    return QuestAgent()
