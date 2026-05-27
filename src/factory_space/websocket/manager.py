"""WebSocket connection manager."""

from __future__ import annotations

from fastapi import WebSocket


class WebSocketConnectionManager:
    """Tracks active WebSocket clients."""

    def __init__(self) -> None:
        self._connections: dict[str, WebSocket] = {}

    async def connect(self, client_id: str, websocket: WebSocket) -> None:
        """Accept and track a WebSocket connection."""

        await websocket.accept()
        self._connections[client_id] = websocket

    def disconnect(self, client_id: str) -> None:
        """Remove a WebSocket connection if it is tracked."""

        self._connections.pop(client_id, None)

    def is_connected(self, client_id: str) -> bool:
        """Return whether a client is currently connected."""

        return client_id in self._connections
