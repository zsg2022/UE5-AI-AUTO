"""快速诊断 8 个失败项的根因"""
import socket, json, uuid, time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 9876))
s.settimeout(15)

def cmd(method, params={}):
    rid = str(uuid.uuid4())
    msg = json.dumps({"id": rid, "method": method, "params": params}) + "\n"
    s.sendall(msg.encode())
    time.sleep(0.1)
    buf = b""
    while True:
        try:
            chunk = s.recv(8192)
            if not chunk: return None
            buf += chunk
            if b"\n" in buf: break
        except socket.timeout: return None
    return json.loads(buf.split(b"\n")[0].decode())

s.sendall(b'{"type":"mcp"}\n')
time.sleep(0.2)
s.recv(4096)

print("=== 诊断 1: CreateBlackboard 是 stub ===")
r = cmd("create_blackboard", {"path": "/Game/Test/DiagBB", "name": "DiagBB"})
print(f"  {r}")

print("\n=== 诊断 2: CreateBehaviorTree 产物能否加载 ===")
r = cmd("bt_create", {"path": "/Game/Test/DiagBT", "name": "DiagBT"})
print(f"  bt_create: {r}")
r = cmd("bt_add_composite", {"path": "/Game/Test/DiagBT.DiagBT", "parent_id": "", "composite_type": "Selector"})
print(f"  用完整路径: {r}")
r = cmd("bt_add_composite", {"path": "/Game/Test/DiagBT", "parent_id": "", "composite_type": "Selector"})
print(f"  用短路径: {r}")

print("\n=== 诊断 3: K2Node 类名查找 ===")
r = cmd("bp_create", {"path": "/Game/Test/DiagBP", "parent_class": "Actor"})
print(f"  bp_create: {r}")
for nc in ["PrintString", "K2Node_PrintString", "K2Node_CallFunction"]:
    r = cmd("bp_create_node", {"path": "/Game/Test/DiagBP", "graph": "EventGraph", "node_class": nc, "x": 0, "y": 0, "defaults": {}} if nc != "K2Node_CallFunction" else {"path": "/Game/Test/DiagBP", "graph": "EventGraph", "node_class": nc, "x": 0, "y": 0, "defaults": {"function": "PrintString"}})
    print(f"  {nc}: {r}")

print("\n=== 诊断 4: PointLight 属性名 ===")
r = cmd("create_actor", {"class_name": "PointLight", "location": {"x": 700, "y": 700, "z": 100}})
res = r.get("result", {})
an = res.get("actor_name", "")
print(f"  actor: {an}")
for pn in ["Intensity", "IntensityUnits", "LightIntensity", "Brightness"]:
    r2 = cmd("modify_actor_property", {"actor_name": an, "property_name": pn, "value": 8000.0})
    print(f"  {pn}: {r2}")

print("\n=== 诊断 5: Sound 资源路径 ===")
r = cmd("create_actor", {"class_name": "StaticMeshActor", "location": {"x": 800, "y": 800, "z": 100}})
an2 = r.get("result", {}).get("actor_name", "")
for sp in ["/Engine/BasicSounds/Notification/Notification_01", "/Engine/EditorSounds/Notifications/CompileSuccess", "/Engine/EngineSounds/A_Test"]:
    r2 = cmd("play_sound", {"actor_name": an2, "sound_path": sp})
    print(f"  {sp}: {r2}")

s.close()