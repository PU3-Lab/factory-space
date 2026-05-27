"""Quest agent entrypoint."""

from __future__ import annotations

from factory_space.core.actions.schemas import Action
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import (
    AgentRequest,
    AgentResponse,
    AgentResponsePayload,
)


class QuestAgent:
    """Default quest agent stub."""

    agent_id = "quest"

    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        """Return a stable placeholder response for integration work."""

        return AgentResponse(
            session_id=context.session_id,
            request_id=context.request_id,
            client_id=context.client_id,
            agent=self.agent_id,
            payload=AgentResponsePayload(
                text="퀘스트 agent가 요청을 수신했습니다.",
                actions=[
                    Action(
                        name="show_ui_message",
                        args={"text": "퀘스트 요청을 확인했습니다."},
                    )
                ],
                metadata={"status": "stub", "received_payload": request.payload},
            ),
        )


def create_agent() -> QuestAgent:
    """Create the default quest agent."""

    return QuestAgent()
