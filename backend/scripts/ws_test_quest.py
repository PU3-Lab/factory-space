import asyncio
import json

from websockets.asyncio.client import connect


async def main() -> None:
    uri = "ws://127.0.0.1:8000/ws"

    async with connect(uri) as websocket:
        await websocket.send(
            json.dumps(
                {
                    "type": "agent_request",
                    "version": "1.0",
                    "request_id": "req-quest-001",
                    "session_id": "test-session",
                    "client_id": "test-client",
                    "agent": "quest",
                    "payload": {
                        "query": "새 퀘스트를 생성해줘.",
                    },
                }
            )
        )
        response = await websocket.recv()
        print(response)


if __name__ == "__main__":
    asyncio.run(main())
