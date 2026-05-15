"""UE5AIAUTO 全量测试 v2 — 含修复版"""
import socket, json, uuid, time

class Bridge:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect(("127.0.0.1", 9876))
        self.sock.settimeout(20)
        self.pass_cnt = self.fail_cnt = self.skip_cnt = 0

    def send(self, method, params={}):
        rid = str(uuid.uuid4())
        self.sock.sendall((json.dumps({"id":rid,"method":method,"params":params})+"\n").encode())
        time.sleep(0.1)
        buf = b""
        while True:
            try:
                c = self.sock.recv(8192)
                if not c: return None
                buf += c
                if b"\n" in buf: break
            except socket.timeout: return None
        return json.loads(buf.split(b"\n")[0].decode())

    def test(self, name, r):
        if r is None: print(f"  FAIL {name}: No response"); self.fail_cnt += 1; return None
        res = r.get("result", {})
        if res.get("error"): print(f"  FAIL {name}: {res['error'][:80]}"); self.fail_cnt += 1; return None
        print(f"  OK   {name}")
        self.pass_cnt += 1
        return res

    def skip(self, name, reason):
        print(f"  SKIP {name}: {reason}")
        self.skip_cnt += 1

    def init(self):
        self.sock.sendall(b'{"type":"mcp"}\n')
        time.sleep(0.2)
        self.sock.recv(4096)
        print("=== UE5AIAUTO 全量测试 v2 ===\n")

    def report(self):
        print(f"\n{'='*50}")
        print(f"  PASS: {self.pass_cnt}  FAIL: {self.fail_cnt}  SKIP: {self.skip_cnt}")
        print(f"{'='*50}")

b = Bridge()
b.init()

# ===== Setup =====
print("--- 搭建测试资产 ---")
BP_PATH = "/Game/Test/BP_TV2"
BP_DIR = "/Game/Test"
res = b.test("bp_create(Actor)", b.send("bp_create", {"path":BP_PATH,"parent_class":"Actor"}))

res = b.test("bp_create_variable(float)", b.send("bp_create_variable", {"path":BP_PATH,"name":"MyFloat","type":"float","is_array":False}))
res = b.test("bp_create_function", b.send("bp_create_function", {"path":BP_PATH,"name":"MyFunc","inputs":[],"outputs":[]}))
res = b.test("bp_add_event_dispatcher", b.send("bp_add_event_dispatcher", {"path":BP_PATH,"name":"MyDisp"}))
res = b.test("bp_create_node(PrintString)", b.send("bp_create_node", {"path":BP_PATH,"graph":"EventGraph","node_class":"PrintString","x":0,"y":0,"defaults":{}}))
node_id = res.get("id","") if res else ""
res = b.test("bp_create_macro", b.send("bp_create_macro", {"path":BP_PATH,"name":"MyMacro"}))
res = b.test("bp_compile", b.send("bp_compile", {"path":BP_PATH}))

# ===== Phase A: BP list/remove =====
print("\n--- 批次A: 蓝图列表/删除 ---")
b.test("bp_list_variables", b.send("bp_list_variables", {"path":BP_PATH}))
b.test("bp_set_variable_default", b.send("bp_set_variable_default", {"path":BP_PATH,"name":"MyFloat","value":"3.14"}))
b.test("bp_list_functions", b.send("bp_list_functions", {"path":BP_PATH}))
b.test("bp_list_graphs", b.send("bp_list_graphs", {"path":BP_PATH}))
b.test("bp_remove_node", b.send("bp_remove_node", {"path":BP_PATH,"graph":"EventGraph","id":node_id})) if node_id else b.skip("bp_remove_node","no node_id")
b.test("bp_remove_variable", b.send("bp_remove_variable", {"path":BP_PATH,"name":"MyFloat"}))
b.test("bp_remove_function", b.send("bp_remove_function", {"path":BP_PATH,"name":"MyFunc"}))
b.test("bp_remove_macro", b.send("bp_remove_macro", {"path":BP_PATH,"name":"MyMacro"}))
b.test("bp_remove_event_dispatcher", b.send("bp_remove_event_dispatcher", {"path":BP_PATH,"name":"MyDisp"}))

# ===== Phase B: BT =====
print("\n--- 批次B: 行为树子命令 ---")
BB_PATH = BP_DIR + "/BB_TV2"
BT_PATH = BP_DIR + "/BT_TV2"
res = b.test("create_blackboard", b.send("create_blackboard", {"path":BB_PATH,"name":"BB_TV2"}))
if res:
    res2 = b.test("add_bb_key", b.send("add_bb_key", {"path":BB_PATH,"key_name":"MyTarget","key_type":"Object"}))
    res3 = b.test("bt_create", b.send("bt_create", {"path":BT_PATH,"name":"BT_TV2"}))
    if res3:
        r4 = b.test("bt_add_composite", b.send("bt_add_composite", {"path":BT_PATH,"parent_id":"","composite_type":"Selector"}))
        comp_id = r4.get("node_id","") if r4 else ""
        if comp_id:
            r5 = b.test("bt_add_task", b.send("bt_add_task", {"path":BT_PATH,"parent_id":comp_id,"task_class":"BTTask_Wait"}))
            task_id = r5.get("node_id","") if r5 else ""
            r6 = b.test("bt_add_decorator", b.send("bt_add_decorator", {"path":BT_PATH,"node_id":task_id,"decorator_class":"BTDecorator_Blackboard"}))
            b.test("bt_add_service", b.send("bt_add_service", {"path":BT_PATH,"composite_id":comp_id,"service_class":"BTService_BlackboardBase"}))

# ===== Phase C: AnimBP =====
print("\n--- 批次C: 动画蓝图 ---")
ABP_PATH = BP_DIR + "/ABP_TV2"
res = b.test("bp_create(AnimBP)", b.send("bp_create", {"path":ABP_PATH,"parent_class":"AnimInstance"}))
if res:
    b.test("anim_set_graph_node", b.send("anim_set_graph_node", {"path":ABP_PATH,"node_class":"AnimGraphNode_StateMachine","x":300,"y":200}))

# ===== Phase D: C++ tools =====
print("\n--- 批次D: C++工具 ---")
res = b.test("create_actor(StaticMesh)", b.send("create_actor", {"class_name":"StaticMeshActor","location":{"x":500,"y":500,"z":100}}))
print(f"  DEBUG create_actor response keys: {list(res.keys()) if res else 'None'}")
an = res.get("actor_name","") if res else ""
print(f"  DEBUG actor_name='{an}'")
if an:
    b.test("set_mass", b.send("set_mass", {"actor_name":an,"mass":50.0}))
    b.test("play_sound", b.send("play_sound", {"actor_name":an,"sound_path":"/Engine/EditorSounds/Notifications/CompileSuccess"}))
    b.test("find_path", b.send("find_path", {"actor_name":an,"x":2000,"y":2000,"z":100}))
b.test("add_axis_mapping", b.send("add_axis_mapping", {"axis_name":"TestAxis","key":"W","scale":1.0}))

# ===== Phase E: modify_actor_property =====
print("\n--- 批次E: Actor属性修改 ---")
b.test("modify_actor_property", b.send("modify_actor_property", {"actor_name":an,"property_name":"bHidden","value":False})) if an else b.skip("modify_actor_property","no actor")

# ===== Phase F: Assets =====
print("\n--- 批次F: 资产/高级 ---")
b.test("create_material", b.send("create_material", {"asset_path":BP_DIR+"/M_TV2","material_type":"opaque","base_color_hex":"FF0000FF"}))
b.test("create_metasound", b.send("create_metasound", {"path":BP_DIR+"/MS_TV2","name":"MS_TV2"}))
b.test("copy_asset", b.send("copy_asset", {"source_path":BP_PATH,"dest_path":BP_DIR+"/BP_TV2_Copy"}))
b.test("list_assets", b.send("list_assets", {"path":BP_DIR,"class_name":""}))
b.test("delete_asset", b.send("delete_asset", {"path":BP_DIR+"/BP_TV2_Copy"}))

# ===== Phase G: NEW commands =====
print("\n--- 批次G: 新增命令 ---")
if an:
    b.test("apply_force", b.send("apply_force", {"actor_name":an,"x":0,"y":0,"z":980.0}))
    b.test("stop_sound", b.send("stop_sound", {"actor_name":an}))

b.report()