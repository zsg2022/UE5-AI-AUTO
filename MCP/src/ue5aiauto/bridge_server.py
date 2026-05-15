"""Bridge TCP Server - relay between MCP and UE5."""
import asyncio, json, logging
logging.basicConfig(level=logging.INFO, format="[%(asctime)s] %(message)s", datefmt="%H:%M:%S")
log = logging.getLogger("bridge")

UE5 = None; PENDING = {}; UE5_LOCK = asyncio.Lock()

async def handle(reader, writer):
    global UE5
    buf = b""
    try:
        while True:
            chunk = await reader.read(65536)
            if not chunk: break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.replace(b"\r", b"").strip()
                if not line: continue
                try: msg = json.loads(line.decode())
                except: continue

                mid = msg.get("id", "").lower()
                method = msg.get("method", "")
                tp = msg.get("type", "")

                if tp == "ue5":
                    async with UE5_LOCK:
                        UE5 = writer
                    log.info("UE5 connected"); continue
                if tp == "mcp":
                    log.info("MCP connected")
                    writer.write(b'{"status":"ok"}\n'); await writer.drain()
                    continue

                if method:
                    async with UE5_LOCK:
                        ue5_writer = UE5
                    if not ue5_writer:
                        resp = json.dumps({"id": msg.get("id"), "error": "UE5 not connected", "error_code": -5}) + "\n"
                        writer.write(resp.encode()); await writer.drain()
                        continue
                    fut = asyncio.get_event_loop().create_future()
                    PENDING[mid] = fut
                    fwd = json.dumps({"id": msg.get("id"), "method": method, "params": msg.get("params", {})}) + "\n"
                    ue5_writer.write(fwd.encode()); await ue5_writer.drain()
                    try:
                        result = await asyncio.wait_for(fut, timeout=30)
                        resp = json.dumps({"id": msg.get("id"), "result": result}) + "\n"
                    except asyncio.TimeoutError:
                        PENDING.pop(mid, None)
                        resp = json.dumps({"id": msg.get("id"), "error": "timeout", "error_code": -4}) + "\n"
                    writer.write(resp.encode()); await writer.drain()
                    log.info("%s: OK", method)
                elif mid in PENDING:
                    PENDING.pop(mid).set_result(msg.get("result", msg))
    except (ConnectionResetError, asyncio.IncompleteReadError, BrokenPipeError):
        pass
    finally:
        writer.close()
        async with UE5_LOCK:
            if writer is UE5:
                UE5 = None

async def main():
    server = await asyncio.start_server(handle, "127.0.0.1", 9876)
    log.info("Bridge on 127.0.0.1:9876")
    async with server: await server.serve_forever()
asyncio.run(main())
