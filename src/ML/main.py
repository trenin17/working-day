import asyncio
import json
from pathlib import Path
import sys

import yaml
from websockets import connect
from websockets.server import serve

def Predict(uid: str | None, data: dict | None) -> dict:
    return f"Touch the grass {uid} {len(data)}"


async def handler(ws, message: str) -> None:
    print(f"Client connected: {websocket.remote_address}")
    try:
        async for message in websocket:
            try:
                payload = json.loads(message)
                uid = payload.get("user_id")
                data = payload.get("data")

                result = await asyncio.to_thread(predict, uid, data)
                await websocket.send(json.dumps(result))
            except json.JSONDecodeError:
                await websocket.send(json.dumps({"status": "error", "message": "Invalid JSON"}))
            except Exception as e:
                await websocket.send(json.dumps({"status": "error", "message": str(e)}))
    except Exception as e:
        print(f"Connection error: {e}")
    finally:
        print(f"Client disconnected: {websocket.remote_address}")


async def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 65432
    print(f"🚀 Starting ML WebSocket server on ws://localhost:{port}")

    async with serve(handler, "127.0.0.1", port):
        await asyncio.get_running_loop().create_future()

if __name__ == "__main__":
    asyncio.run(main())