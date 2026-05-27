"""Material generation agent entrypoint."""

from __future__ import annotations

from factory_space.core.actions.schemas import Action
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import (
    AgentRequest,
    AgentResponse,
    AgentResponsePayload,
)


class MaterialGenerationAgent:
    """Default material generation agent stub."""

    agent_id = "material_generation"

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
                text="신물질 생성 agent가 요청을 수신했습니다.",
                actions=[
                    Action(
                        name="show_ui_message",
                        args={"text": "신물질 생성 요청을 확인했습니다."},
                    )
                ],
                metadata={"status": "stub", "received_payload": request.payload},
            ),
        )


def create_agent() -> MaterialGenerationAgent:
    """Create the default material generation agent."""

    return MaterialGenerationAgent()
