import asyncio
import json

from websockets.asyncio.client import connect


async def main() -> None:
    uri = "ws://127.0.0.1:18000/ws/agent"

    async with connect(uri) as websocket:
        await websocket.send(
            json.dumps(
                {
                    "type": "agent.request",
                    "version": "1.0",
                    "request_id": "req-operator-guide-001",
                    "session_id": "test-session",
                    "client_id": "test-client",
                    "agent": "operator_guide",
                    "payload": {
                        "question": "How do I use this machine?",
                    },
                }
            )
        )
        response = await websocket.recv()
        print(response)


if __name__ == "__main__":
    asyncio.run(main())

