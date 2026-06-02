"""WebSocket gateway for agent requests."""

from __future__ import annotations

import json
from typing import cast

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from agents.pipeline import AgentPipeline
from protocol.errors import build_error_payload

router = APIRouter()


def get_agent_pipeline(websocket: WebSocket) -> AgentPipeline:
    """Return the application-scoped agent pipeline."""

    app = websocket.scope["app"]
    return cast(AgentPipeline, app.state.agent_pipeline)


@router.websocket("/ws/agent")
async def agent_websocket(websocket: WebSocket) -> None:
    """Accept the agent WebSocket connection."""

    await websocket.accept()
    pipeline = get_agent_pipeline(websocket)
    try:
        while True:
            raw_message = await websocket.receive_text()
            try:
                message = json.loads(raw_message)
            except json.JSONDecodeError:
                await websocket.send_json(
                    {
                        "type": "agent.error",
                        "error": build_error_payload(
                            "INVALID_JSON",
                            "WebSocket message must be valid JSON.",
                        ),
                    }
                )
                continue

            await websocket.send_json(pipeline.run(message))
    except WebSocketDisconnect:
        return
