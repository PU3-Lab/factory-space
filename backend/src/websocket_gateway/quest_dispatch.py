"""WebSocket quest dispatcher for processing quest-related events."""

from __future__ import annotations

import asyncio
import logging
from collections.abc import Callable, Coroutine
from typing import Any
from uuid import uuid4

from fastapi import WebSocket

from agents.quest_generator.compose_service import (
    ComposeResult,
    compose_first_support_quest,
    get_phrase_refiner,
)
from db.engine import get_db_session
from protocol.errors import build_error_payload

logger = logging.getLogger(__name__)


async def dispatch_quest_message(
    websocket: WebSocket,
    message: dict[str, Any],
    send_json: Callable[[WebSocket, dict[str, Any]], Coroutine[Any, Any, None]],
) -> None:
    """Quest 관련 WebSocket 메시지를 라우팅하고 처리합니다.

    초보자용 설명:
        이 디스패처는 클라이언트(예: Unreal Engine)가 보낸 WebSocket 메시지 중
        'quest.' 접두사로 시작하는 메시지(예: quest.tutorial_completed)를 가로채어 처리합니다.
        메인 WebSocket 스레드를 차단하지 않도록 DB 트랜잭션 및 퀘스트 생성 연산을
        `asyncio.to_thread`를 사용해 백그라운드 스레드에서 수행합니다.
        성공/실패 시 요청의 request_id, session_id, client_id를 그대로 반향(echo)하여 응답합니다.

    노트 (멱등성 한계):
        현재 구현은 request_id를 응답에 echo 처리하여 돌려주기만 할 뿐, 서버 자체에
        중복 요청을 판별하는 멱등성 저장소/기록은 가지고 있지 않습니다.
        네트워크 재전송 상황에서 완벽한 멱등성 보장이 필요하다면 후속 과제로
        request_id 처리 기록 저장 로직을 도입해야 합니다.
    """
    message_type = message.get("type")
    request_id = message.get("request_id") or str(uuid4())
    session_id = message.get("session_id")
    client_id = message.get("client_id")

    # 공통 응답 봉투(Envelope) 구성을 위한 헬퍼 클로저
    def make_response(
        resp_type: str,
        payload: dict[str, Any] | None = None,
        error: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        resp: dict[str, Any] = {
            "type": resp_type,
            "request_id": request_id,
        }
        if session_id is not None:
            resp["session_id"] = session_id
        if client_id is not None:
            resp["client_id"] = client_id
        if payload is not None:
            resp["payload"] = payload
        if error is not None:
            resp["error"] = error
        return resp

    if message_type == "quest.tutorial_completed":
        payload = message.get("payload")
        if not isinstance(payload, dict):
            err_payload = make_response(
                "agent.error",
                error=build_error_payload(
                    "INVALID_PAYLOAD", "Payload must be a dictionary."
                ),
            )
            await send_json(websocket, err_payload)
            return

        context_payload = payload.get("context")
        if not isinstance(context_payload, dict):
            err_payload = make_response(
                "agent.error",
                error=build_error_payload(
                    "INVALID_PAYLOAD", "Payload.context must be a dictionary."
                ),
            )
            await send_json(websocket, err_payload)
            return

        factory_id = context_payload.get("factory_id")
        if not factory_id or not isinstance(factory_id, str):
            err_payload = make_response(
                "agent.error",
                error=build_error_payload(
                    "INVALID_PAYLOAD",
                    "Payload.context.factory_id must be a non-empty string.",
                ),
            )
            await send_json(websocket, err_payload)
            return

        # 동기 compose 연산을 asyncio.to_thread를 이용해 워커 스레드에서 비차단 실행
        try:
            phrase_refiner = get_phrase_refiner()

            def run_compose() -> ComposeResult:
                with get_db_session() as session:
                    return compose_first_support_quest(
                        session=session,
                        factory_id=factory_id,
                        context_payload=context_payload,
                        phrase_refiner=phrase_refiner,
                    )

            result = await asyncio.to_thread(run_compose)

            if result.outcome == "composed":
                if result.instance is None:
                    raise ValueError(
                        "Compose outcome was 'composed' but instance is None"
                    )
                instance_dict = result.instance.model_dump(mode="json")
                resp = make_response("quest.composed", payload=instance_dict)
                await send_json(websocket, resp)
            else:
                resp = make_response("quest.none", payload={"reason": result.reason})
                await send_json(websocket, resp)

        except Exception:
            logger.exception("Failed to process quest.tutorial_completed")
            err_payload = make_response(
                "agent.error",
                error=build_error_payload(
                    "INTERNAL_ERROR",
                    "An internal error occurred while processing the request.",
                ),
            )
            await send_json(websocket, err_payload)
    else:
        err_payload = make_response(
            "agent.error",
            error=build_error_payload(
                "UNKNOWN_MESSAGE_TYPE",
                f"Unknown quest message type: {message_type}",
            ),
        )
        await send_json(websocket, err_payload)
