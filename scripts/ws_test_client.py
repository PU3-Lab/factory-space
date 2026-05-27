import asyncio
import json

from websockets.asyncio.client import connect


async def main() -> None:
    uri = "ws://127.0.0.1:8000/ws/test-client"

    async with connect(uri) as websocket:
        await websocket.send(
            json.dumps(
                {
                    "type": "ping",
                    "version": "1.0",
                    "session_id": "test-session",
                    "payload": {"timestamp": "now"},
                }
            )
        )

        response = await websocket.recv()
        print(response)


if __name__ == "__main__":
    asyncio.run(main())