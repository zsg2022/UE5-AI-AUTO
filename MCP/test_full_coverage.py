"""UE5AIAUTO 全量测试脚本 — 测试所有未覆盖命令"""
import socket, json, uuid, time, sys

class Bridge:
    def __init__(self, host="127.0.0.1", port=9876, timeout=20):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.connect((host, port))
        self.s.settimeout(timeout)
        self.results = {"pass": [], "fail": [], "skip": []}

    def _send(self, method, params={}):
        rid = str(uuid.uuid4())
        msg = json.dumps({"id": rid, "method": method, "params": params}) + "\n"
        self.s.sendall(msg.encode())
        time.sleep(0.1)
        buf = b""
        while True:
            try:
                chunk = self.s.recv(8192)
                if not chunk: return None
                buf += chunk
                if b"\n" in buf: break
            except socket.timeout: return None
        return json.loads(buf.split(b"\n")[0].decode())

    def cmd(self, method, params={}):
        return self._send(method, params)

    def ok(self, name, r):
        if r is None:
            self.results["fail"].append((name, "No response"))
            return False
        res = r.get("result", {})
        err = res.get("error", "")
        if err:
            self.results["fail"].append((name, err[:100]))
            return False
        return True

    def skip(self, name, reason):
        self.results["skip"].append((name, reason))

    def done(self, name):
        self.results["pass"].append((name, "OK"))

    def register(self):
        self.s.sendall(b'{"type":"mcp"}\n')
        time.sleep(0.2)
        chunk = self.s.recv(4096)
        return b"ok" in chunk

    def close(self):
        self.s.close()

    def report(self):
        p, f, s = len(self.results["pass"]), len(self.results["fail"]), len(self.results["skip"])
        print(f"\n{'='*60}")
        print(f"  总计: {p+f+s} | PASS: {p} | FAIL: {f} | SKIP: {s}")
        print(f"{'='*60}")
        if self.results["fail"]:
            print("\n失败项:")
            for n, e in self.results["fail"]:
                print(f"  FAIL: {n} → {e}")


def run_all():
    b = Bridge()
    if not b.register():
        print("Bridge 注册失败!")
        b.close()
        return
    print("Bridge connected\n")

    # ========== Phase 2.1: 搭建测试蓝图 ==========
    print("--- Phase 2.1: 搭建测试蓝图 ---")
    
    # 创建测试用蓝图
    r = b.cmd("bp_create", {"path": "/Game/Test/BP_TestAll", "parent_class": "Actor"})
    if b.ok("bp_create", r):
        b.done("bp_create_测试蓝图")
    else:
        print("无法创建测试蓝图，终止")
        b.close()
        return

    # 创建变量
    r = b.cmd("bp_create_variable", {"path": "/Game/Test/BP_TestAll", "name": "TestFloat", "type": "float", "is_array": False})
    b.ok("bp_create_variable", r)
    r = b.cmd("bp_create_variable", {"path": "/Game/Test/BP_TestAll", "name": "TestInt", "type": "int", "is_array": False})
    b.ok("bp_create_variable_2", r)

    # 创建函数
    r = b.cmd("bp_create_function", {"path": "/Game/Test/BP_TestAll", "name": "TestFunction", "inputs": [], "outputs": []})
    b.ok("bp_create_function", r)

    # 添加事件分发器
    r = b.cmd("bp_add_event_dispatcher", {"path": "/Game/Test/BP_TestAll", "name": "TestDispatcher"})
    b.ok("bp_add_event_dispatcher", r)

    # 添加节点到EventGraph
    r = b.cmd("bp_create_node", {"path": "/Game/Test/BP_TestAll", "graph": "EventGraph", "node_class": "PrintString", "x": 0, "y": 0, "defaults": {}})
    if b.ok("bp_create_node", r):
        res = r.get("result", {})
        node_id = res.get("node_id", "")
    else:
        node_id = ""

    # 创建宏（先获取path）
    r = b.cmd("bp_create_macro", {"path": "/Game/Test/BP_TestAll", "name": "TestMacro"})
    b.ok("bp_create_macro", r)

    # 编译
    r = b.cmd("bp_compile", {"path": "/Game/Test/BP_TestAll"})
    b.ok("bp_compile", r)

    # ========== Phase 2.2: 批次 A — 蓝图列表/删除类（10个）==========
    print("\n--- Phase 2.2: 批次 A — 蓝图列表/删除类 ---")

    r = b.cmd("bp_list_variables", {"path": "/Game/Test/BP_TestAll"})
    b.ok("bp_list_variables", r)

    r = b.cmd("bp_set_variable_default", {"path": "/Game/Test/BP_TestAll", "name": "TestFloat", "value": "1.5"})
    b.ok("bp_set_variable_default", r)

    r = b.cmd("bp_list_functions", {"path": "/Game/Test/BP_TestAll"})
    b.ok("bp_list_functions", r)

    r = b.cmd("bp_list_graphs", {"path": "/Game/Test/BP_TestAll"})
    b.ok("bp_list_graphs", r)

    # remove node
    if node_id:
        r = b.cmd("bp_remove_node", {"path": "/Game/Test/BP_TestAll", "graph": "EventGraph", "node_id": node_id})
        b.ok("bp_remove_node", r)
    else:
        b.skip("bp_remove_node", "无有效node_id")

    # remove variable
    r = b.cmd("bp_remove_variable", {"path": "/Game/Test/BP_TestAll", "name": "TestInt"})
    b.ok("bp_remove_variable", r)

    # remove function
    r = b.cmd("bp_remove_function", {"path": "/Game/Test/BP_TestAll", "name": "TestFunction"})
    b.ok("bp_remove_function", r)

    # remove macro
    r = b.cmd("bp_remove_macro", {"path": "/Game/Test/BP_TestAll", "name": "TestMacro"})
    b.ok("bp_remove_macro", r)

    # remove event dispatcher
    r = b.cmd("bp_remove_event_dispatcher", {"path": "/Game/Test/BP_TestAll", "name": "TestDispatcher"})
    b.ok("bp_remove_event_dispatcher", r)

    # ========== Phase 2.3: 批次 B — 行为树子命令（5个）==========
    print("\n--- Phase 2.3: 批次 B — 行为树子命令 ---")

    r = b.cmd("create_blackboard", {"path": "/Game/Test/BB_Test", "name": "BB_Test"})
    if b.ok("create_blackboard", r):
        r = b.cmd("add_bb_key", {"path": "/Game/Test/BB_Test", "key_name": "Target", "key_type": "Object"})
        b.ok("add_bb_key", r)

        r = b.cmd("bt_create", {"path": "/Game/Test/BT_Test", "name": "BT_Test"})
        if b.ok("bt_create", r):
            # bt_add_composite (root)
            r = b.cmd("bt_add_composite", {"path": "/Game/Test/BT_Test", "parent_id": "", "composite_type": "Selector"})
            if b.ok("bt_add_composite", r):
                res = r.get("result", {})
                comp_id = res.get("node_id", "")
            else:
                comp_id = ""

            # bt_add_task
            r = b.cmd("bt_add_task", {"path": "/Game/Test/BT_Test", "parent_id": comp_id, "task_class": "BTTask_Wait"})
            if b.ok("bt_add_task", r):
                res = r.get("result", {})
                task_id = res.get("node_id", "")
            else:
                task_id = ""

            # bt_add_decorator
            if task_id:
                r = b.cmd("bt_add_decorator", {"path": "/Game/Test/BT_Test", "node_id": task_id, "decorator_class": "BTDecorator_Blackboard"})
                b.ok("bt_add_decorator", r)
            else:
                b.skip("bt_add_decorator", "无有效task_id")

            # bt_add_service
            if comp_id:
                r = b.cmd("bt_add_service", {"path": "/Game/Test/BT_Test", "composite_id": comp_id, "service_class": "BTService_BlackboardBase"})
                b.ok("bt_add_service", r)
            else:
                b.skip("bt_add_service", "无有效composite_id")
    else:
        b.skip("行为树批次", "create_blackboard失败")

    # ========== Phase 2.4: 批次 C — 动画蓝图 ==========
    print("\n--- Phase 2.4: 批次 C — 动画蓝图 ---")

    r = b.cmd("bp_create", {"path": "/Game/Test/ABP_Test", "parent_class": "AnimInstance"})
    if b.ok("bp_create_AnimBP", r):
        r = b.cmd("anim_set_graph_node", {"path": "/Game/Test/ABP_Test", "node_class": "AnimGraphNode_StateMachine", "x": 300, "y": 200})
        b.ok("anim_set_graph_node", r)
    else:
        b.skip("anim_set_graph_node", "AnimBP创建失败")

    # ========== Phase 2.5: 批次 D — C++ 工具 ==========
    print("\n--- Phase 2.5: 批次 D — C++ 工具 ---")

    r = b.cmd("create_actor", {"class_name": "StaticMeshActor", "location": {"x": 500, "y": 500, "z": 100}})
    if b.ok("create_actor_tools", r):
        actor_name = r.get("result", {}).get("actor_name", "")

        r = b.cmd("set_mass", {"actor_name": actor_name, "mass": 100.0})
        b.ok("set_mass", r)

        r = b.cmd("play_sound", {"actor_name": actor_name, "sound_path": "/Engine/BasicSounds/Notification/Notification_01"})
        b.ok("play_sound", r)

        r = b.cmd("find_path", {"actor_name": actor_name, "x": 1000, "y": 1000, "z": 100})
        b.ok("find_path", r)
    else:
        b.skip("C++工具批次", "create_actor失败")

    r = b.cmd("add_axis_mapping", {"axis_name": "MoveForward", "key": "W", "scale": 1.0})
    b.ok("add_axis_mapping", r)

    # ========== Phase 2.6: 批次 E — Actor 属性修改 ==========
    print("\n--- Phase 2.6: 批次 E — Actor 属性修改 ---")

    r = b.cmd("create_actor", {"class_name": "PointLight", "location": {"x": 600, "y": 600, "z": 200}})
    if b.ok("create_actor_mod", r):
        actor_name = r.get("result", {}).get("actor_name", "")
        r = b.cmd("modify_actor_property", {"actor_name": actor_name, "property_name": "Intensity", "value": 8000.0})
        b.ok("modify_actor_property", r)
    else:
        b.skip("modify_actor_property", "create_actor失败")

    # ========== Phase 2.7: 批次 F — 资产/高级 ==========
    print("\n--- Phase 2.7: 批次 F — 资产/高级 ---")

    r = b.cmd("create_material", {"asset_path": "/Game/Test/M_Test", "material_type": "opaque", "base_color_hex": "FF0000FF"})
    b.ok("create_material", r)

    r = b.cmd("create_metasound", {"path": "/Game/Test/MS_Test", "name": "MS_Test"})
    b.ok("create_metasound", r)

    r = b.cmd("copy_asset", {"source_path": "/Game/Test/BP_TestAll", "dest_path": "/Game/Test/BP_TestAll_Copy"})
    b.ok("copy_asset", r)

    r = b.cmd("list_assets", {"path": "/Game/Test", "class_name": ""})
    b.ok("list_assets", r)

    r = b.cmd("delete_asset", {"path": "/Game/Test/BP_TestAll_Copy"})
    b.ok("delete_asset", r)

    # ========== 报告 ==========
    b.report()
    b.close()

if __name__ == "__main__":
    run_all()