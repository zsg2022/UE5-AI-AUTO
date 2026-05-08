"""Systematic test of all UE5AIAUTO commands."""
import asyncio, json, uuid, time

PASS = 0; FAIL = 0

async def send_cmd(r, w, method, params=None, timeout=30):
    rid = str(uuid.uuid4())
    msg = json.dumps({"id": rid, "method": method, "params": params or {}})
    w.write((msg + "\n").encode())
    await w.drain()
    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            chunk = await asyncio.wait_for(r.read(65536), timeout=2)
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.replace(b"\r", b"").strip()
                if not line: continue
                try: resp = json.loads(line.decode())
                except: continue
                if resp.get("id") == rid:
                    return resp
        except asyncio.TimeoutError:
            continue
    return {"error": "timeout"}

async def run_cmd(method, params=None):
    """Run a command and return (result_obj, error_str)."""
    global PASS, FAIL
    r, w = await asyncio.open_connection("127.0.0.1", 9876)
    w.write(b'{"type":"mcp"}\n'); await w.drain()
    await asyncio.wait_for(r.readline(), timeout=5)
    resp = await send_cmd(r, w, method, params, timeout=15)
    w.close()
    return resp.get("result"), resp.get("error")

async def test(name, method, params=None):
    global PASS, FAIL
    result, error = await run_cmd(method, params)
    if error:
        print(f"  {name}: FAIL - {error[:100]}")
        FAIL += 1
    else:
        detail = json.dumps(result, ensure_ascii=False)
        if len(detail) > 150: detail = detail[:150] + "..."
        print(f"  {name}: OK - {detail}")
        PASS += 1
    return result, error

async def main():
    global PASS, FAIL
    print("=== UE5 AI Plugin - Full Command Test ===\n")

    # 1. ping
    await test("ping", "ping")

    # 2. create actors (capture real names)
    r1, _ = await test("create_actor (StaticMeshActor)", "create_actor",
           {"class_name": "StaticMeshActor", "location": {"x": 100, "y": 200, "z": 100}})
    r2, _ = await test("create_actor (PointLight)", "create_actor",
           {"class_name": "PointLight", "location": {"x": -100, "y": -200, "z": 300}})

    mesh_name = r1.get("actor_name", "") if r1 else ""
    light_name = r2.get("actor_name", "") if r2 else ""

    # 3. query_scene
    await test("query_scene", "query_scene")

    # 4. modify_actor_property (use real names)
    if light_name:
        await test(f"modify_actor_property ({light_name})", "modify_actor_property",
                   {"actor_name": light_name, "property_name": "Intensity", "value": 5000.0})
    else:
        print("  modify_actor_property: SKIP - no actor name captured")
        FAIL += 1

    # 5. delete_actor (use real names)
    if mesh_name:
        await test(f"delete_actor ({mesh_name})", "delete_actor",
                   {"actor_name": mesh_name})
    else:
        print("  delete_actor: SKIP - no actor name captured")
        FAIL += 1

    # 6. set_viewport_camera
    await test("set_viewport_camera", "set_viewport_camera",
               {"location": {"x": 0, "y": 0, "z": 1000}})

    # 7. scan_reflection
    await test("scan_reflection", "scan_reflection")

    # 8. screenshot
    await test("screenshot", "screenshot",
               {"width": 640, "height": 480, "quality": 50, "format": "jpeg"})

    print(f"\n=== Results: {PASS} pass, {FAIL} fail ===")

asyncio.run(main())
