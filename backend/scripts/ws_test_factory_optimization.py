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
                    "request_id": "req-factory-001",
                    "session_id": "test-session",
                    "client_id": "test-client",
                    "agent": "factory_optimization",
                    "payload": {
                        "query": "공장 최적화 전략을 추천해줘.",
                    },
                }
            )
        )
        response = await websocket.recv()
        print(response)


if __name__ == "__main__":
    asyncio.run(main())
