"""operator_guide WebSocket smoke test 스크립트.

이 스크립트는 브라우저의 `/agent-test` 화면처럼 `/ws/agent`에 직접 연결해서
`operator_guide` 요청을 보내고, 중간 진행 메시지(`agent.progress`)와 최종 응답
(`agent.response`)을 끝까지 확인합니다.

초보자용 설명:
    `agent.progress`는 LLM의 숨은 생각이 아니라, UI에 보여줘도 되는 안전한 상태
    메시지입니다. 예를 들어 "장비 매뉴얼을 펼쳐보는 중입니다..." 같은 문구를
    먼저 받고, 마지막에 실제 답변 JSON을 받는 흐름을 검증합니다.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import uuid
from collections.abc import Sequence
from typing import Any

from websockets.asyncio.client import connect

DEFAULT_URL = "ws://127.0.0.1:18000/ws/agent"
DEFAULT_QUESTION = "분쇄기가 뭐야? 어디에 써?"


class OperatorGuideSmokeError(AssertionError):
    """operator_guide smoke test가 기대한 응답 계약과 다를 때 발생하는 오류입니다."""


async def run_smoke(url: str, question: str, timeout_seconds: float) -> dict[str, Any]:
    """WebSocket으로 operator_guide 질문을 보내고 최종 응답을 반환합니다.

    progress 메시지는 여러 번 올 수 있으므로 최종 `agent.response` 또는
    `agent.error`가 올 때까지 반복해서 수신합니다.
    """

    request_id = f"operator-guide-smoke-{uuid.uuid4().hex[:8]}"
    message = {
        "type": "agent.request",
        "version": "1.0",
        "request_id": request_id,
        "session_id": "operator-guide-smoke-session",
        "client_id": "operator-guide-smoke-client",
        "agent": "operator_guide",
        "payload": {
            "question": question,
        },
        "context": {
            "language": "ko",
            "mode": "smoke",
        },
    }

    async with connect(url) as websocket:
        await websocket.send(json.dumps(message, ensure_ascii=False))
        while True:
            raw_response = await asyncio.wait_for(
                websocket.recv(), timeout=timeout_seconds
            )
            response = _parse_response(raw_response)
            _print_response_summary(response)

            response_type = response.get("type")
            if response_type == "agent.progress":
                continue
            if response_type == "agent.response":
                _validate_operator_guide_response(response)
                return response
            if response_type == "agent.error":
                raise OperatorGuideSmokeError(
                    f"operator_guide smoke returned error: {response}"
                )
            raise OperatorGuideSmokeError(f"Unexpected response type: {response_type}")


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    """명령줄 인자를 읽어 smoke 대상 URL과 질문을 정합니다."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--question", default=DEFAULT_QUESTION)
    parser.add_argument("--timeout", type=float, default=60.0)
    return parser.parse_args(argv)


def _parse_response(raw_response: str) -> dict[str, Any]:
    try:
        response = json.loads(raw_response)
    except json.JSONDecodeError as exc:
        raise OperatorGuideSmokeError(
            f"Response was not valid JSON: {raw_response}"
        ) from exc

    if not isinstance(response, dict):
        raise OperatorGuideSmokeError(f"Response must be a JSON object: {response}")
    return response


def _print_response_summary(response: dict[str, Any]) -> None:
    response_type = response.get("type")
    payload = response.get("payload")
    if response_type == "agent.progress" and isinstance(payload, dict):
        print(f"PROGRESS {payload.get('stage')}: {payload.get('message')}")
        return

    print(json.dumps(response, ensure_ascii=False, indent=2))


def _validate_operator_guide_response(response: dict[str, Any]) -> None:
    if response.get("agent") != "operator_guide":
        raise OperatorGuideSmokeError(f"Expected operator_guide, got {response}")

    payload = response.get("payload")
    if not isinstance(payload, dict):
        raise OperatorGuideSmokeError(f"Expected payload object, got {response}")

    final_answer = payload.get("final_answer")
    if not isinstance(final_answer, str) or not final_answer.strip():
        raise OperatorGuideSmokeError(f"Expected non-empty final_answer, got {payload}")

    metadata = payload.get("metadata")
    if not isinstance(metadata, dict):
        raise OperatorGuideSmokeError(f"Expected metadata object, got {payload}")

    if metadata.get("selectedAgent") != "operator_guide":
        raise OperatorGuideSmokeError(
            f"Expected selectedAgent operator_guide, got {metadata}"
        )


def main(argv: Sequence[str] | None = None) -> int:
    """스크립트 진입점입니다. 성공하면 0, 실패하면 예외로 종료합니다."""

    args = parse_args(argv)
    response = asyncio.run(run_smoke(args.url, args.question, args.timeout))
    print(f"PASS operator_guide smoke: {response['payload']['final_answer']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
