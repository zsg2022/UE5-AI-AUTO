"""Standalone TCP server for testing UE5 connectivity via raw sockets."""
import asyncio
import json
import logging
import sys
sys.path.insert(0, "src")

logging.basicConfig(level=logging.INFO, format="[%(asctime)s] %(levelname)s: %(message)s", datefmt="%H:%M:%S")
logger = logging.getLogger("test")

async def handle_client(reader, writer):
    addr = writer.get_extra_info('peername')
    logger.info("UE5 CONNECTED from %s!", addr)

    try:
        while True:
            line = await reader.readline()
            if not line: break

            try:
                msg = json.loads(line.decode('utf-8').strip())
            except json.JSONDecodeError:
                logger.warning("Invalid JSON: %s", line[:100])
                continue

            method = msg.get("method", "?")
            request_id = msg.get("id", "")
            logger.info("Received: %s (id=%s)", method, request_id)

            # Simple echo response
            resp = {
                "id": request_id,
                "result": {"status": "ok", "method": method, "note": "TCP test server"},
                "error": None
            }
            response_json = json.dumps(resp) + "\n"
            writer.write(response_json.encode('utf-8'))
            await writer.drain()
            logger.info("Sent response for %s", method)
    except ConnectionResetError:
        logger.info("UE5 disconnected")
    finally:
        writer.close()

async def main():
    server = await asyncio.start_server(handle_client, '127.0.0.1', 9876)
    logger.info("TCP server listening on 127.0.0.1:9876 - waiting for UE5...")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())
