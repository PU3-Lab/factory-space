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
                    "request_id": "req-material-001",
                    "session_id": "test-session",
                    "client_id": "test-client",
                    "agent": "material_generation",
                    "payload": {
                        "machine_type": "Synthesizer",
                        "inputs": [
                            {"item_id": "iron_ingot", "qty": 2},
                            {"item_id": "copper_ingot", "qty": 1},
                        ],
                        "process_conditions": {
                            "temperature": "1200C",
                            "pressure": "5atm",
                            "catalyst": "palladium",
                        },
                        "generate_visual_asset": True,
                    },
                }
            )
        )
        response = await websocket.recv()
        print(response)


if __name__ == "__main__":
    asyncio.run(main())
