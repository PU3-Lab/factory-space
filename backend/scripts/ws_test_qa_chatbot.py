import asyncio
import json

from websockets.asyncio.client import connect


async def main() -> None:
    uri = "ws://127.0.0.1:18000/ws/agent"

    async with connect(uri) as websocket:
        await websocket.send(
            json.dumps(
                {
                    "type": "agent_request",
                    "version": "1.0",
                    "request_id": "req-qa-001",
                    "session_id": "test-session",
                    "client_id": "test-client",
                    "agent": "qa_chatbot",
                    "payload": {
                        "query": "제품 사용법을 알려줘.",
                    },
                }
            )
        )
        response = await websocket.recv()
        print(response)


if __name__ == "__main__":
    asyncio.run(main())
