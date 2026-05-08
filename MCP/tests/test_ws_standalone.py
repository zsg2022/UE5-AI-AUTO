"""Standalone WebSocket server for testing UE5 connectivity."""
import asyncio
import json
import logging
import sys
sys.path.insert(0, "src")

from ue5aiauto.config import Config
from ue5aiauto.ws_server import GU5WebSocketServer

logging.basicConfig(level=logging.DEBUG, format="[%(asctime)s] %(levelname)s: %(message)s", datefmt="%H:%M:%S")
logger = logging.getLogger("test")

async def main():
    config = Config.load(port=9876)
    server = GU5WebSocketServer(config)
    await server.start()
    logger.info("WebSocket server running on ws://%s:%d - waiting for UE5...", config.host, config.port)

    try:
        while True:
            await asyncio.sleep(1)
            if server.is_connected:
                logger.info("UE5 CONNECTED!")
                # Send a ping to verify
                result = await server.send_command("ping", {})
                logger.info("Ping result: %s", result)
                break
    except KeyboardInterrupt:
        pass
    finally:
        await server.stop()

if __name__ == "__main__":
    asyncio.run(main())
