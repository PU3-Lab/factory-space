"""WebSocket gateway for agent requests."""

from __future__ import annotations

from fastapi import APIRouter, WebSocket

router = APIRouter()


@router.websocket("/ws/agent")
async def agent_websocket(websocket: WebSocket) -> None:
    """Accept the agent WebSocket connection."""

    await websocket.accept()
    await websocket.close(code=1011)
