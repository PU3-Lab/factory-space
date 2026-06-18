"""WebSocket gateway for agent requests."""

from __future__ import annotations

import asyncio
import json
import logging
from typing import cast
from uuid import uuid4

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from agents.pipeline import AgentPipeline
from protocol.errors import build_error_payload
from websocket_gateway.quest_dispatch import dispatch_quest_message

router = APIRouter()
LOGGER = logging.getLogger("uvicorn.error")
SENSITIVE_LOG_KEYS = frozenset(
    {
        "api_key",
        "authorization",
        "password",
        "secret",
        "token",
    }
)


def get_agent_pipeline(websocket: WebSocket) -> AgentPipeline:
    """Return the application-scoped agent pipeline."""

    app = websocket.scope["app"]
    return cast(AgentPipeline, app.state.agent_pipeline)


def sanitize_for_log(value: object) -> object:
    """Return a log-safe copy of a JSON-compatible value."""

    if isinstance(value, dict):
        sanitized: dict[str, object] = {}
        for raw_key, raw_value in value.items():
            key = str(raw_key)
            if key.lower() in SENSITIVE_LOG_KEYS:
                sanitized[key] = "[redacted]"
            else:
                sanitized[key] = sanitize_for_log(raw_value)
        return sanitized
    if isinstance(value, list):
        return [sanitize_for_log(item) for item in value]
    return value


async def send_agent_json(websocket: WebSocket, message: dict[str, object]) -> None:
    """Log and send an agent WebSocket message."""

    LOGGER.info(
        "Factory agent WebSocket sending: %s",
        json.dumps(sanitize_for_log(message), ensure_ascii=False, sort_keys=True),
    )
    await websocket.send_json(message)


@router.websocket("/ws/agent")
async def agent_websocket(websocket: WebSocket) -> None:
    """WebSocket 연결을 수락하고 에이전트 요청을 처리합니다.

    초보자용 설명:
        클라이언트와 WebSocket 연결을 수립한 뒤, 들어오는 요청(agent.request)을
        LangGraph 파이프라인으로 라우팅하여 실행합니다.
        진행 상황을 실시간으로 스트리밍하기 위해, 동기식인 파이프라인 실행을
        `asyncio.to_thread`를 사용하여 별도 스레드에서 백그라운드로 실행합니다.
        이를 통해 파이프라인 실행 중에도 메인 이벤트 루프가 차단(blocking)되지 않고,
        `agent.progress` 메시지가 스레드 안전하게 실시간으로 클라이언트에게 전송됩니다.
    """

    await websocket.accept()
    pipeline = get_agent_pipeline(websocket)
    try:
        while True:
            raw_message = await websocket.receive_text()
            try:
                message = json.loads(raw_message)
            except json.JSONDecodeError:
                await send_agent_json(
                    websocket,
                    {
                        "type": "agent.error",
                        "error": build_error_payload(
                            "INVALID_JSON",
                            "WebSocket message must be valid JSON.",
                        ),
                    },
                )
                continue

            if not isinstance(message, dict):
                await send_agent_json(
                    websocket,
                    {
                        "type": "agent.error",
                        "error": build_error_payload(
                            "INVALID_ENVELOPE",
                            "WebSocket message must be a JSON object.",
                        ),
                    },
                )
                continue

            message_type = message.get("type")
            if isinstance(message_type, str) and message_type.startswith("quest."):
                await dispatch_quest_message(websocket, message, send_agent_json)
                continue

            loop = asyncio.get_running_loop()

            def handle_progress(stage: str, message_text: str) -> None:
                event = {
                    "type": "agent.progress",
                    "request_id": message.get("request_id") or str(uuid4()),
                    "session_id": message.get("session_id"),
                    "client_id": message.get("client_id"),
                    "agent": message.get("agent") or "operator_guide",
                    "payload": {
                        "stage": stage,
                        "message": message_text,
                    },
                }
                loop.call_soon_threadsafe(
                    lambda: asyncio.create_task(send_agent_json(websocket, event))
                )

            response = await asyncio.to_thread(
                pipeline.run,
                message,
                on_progress=handle_progress,
            )
            await send_agent_json(websocket, response)
    except WebSocketDisconnect:
        return
