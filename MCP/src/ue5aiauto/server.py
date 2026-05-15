"""UE5-AI-AUTO MCP Server - Claude Code integration."""
import argparse, asyncio, json, logging, os, uuid
from fastmcp import FastMCP

logger = logging.getLogger("ue5aiauto.server")
mcp = FastMCP("UE5-AI-AUTO")

_tcp_reader = None; _tcp_writer = None; _tcp_lock = asyncio.Lock(); _pending = {}

@mcp.tool()
async def ping() -> str:
    r = await send("ping"); return f"pong - {r.get('status',r)}"

@mcp.tool()
async def create_actor(class_name: str, location: dict = None, rotation: dict = None, scale: dict = None) -> dict:
    return await send("create_actor", {"class_name": class_name, "location": location or {"x":0,"y":0,"z":100}, "rotation": rotation or {"pitch":0,"yaw":0,"roll":0}, "scale": scale or {"x":1,"y":1,"z":1}})

@mcp.tool()
async def query_scene(filter_class: str = "") -> dict:
    return await send("query_scene", {"filter_class": filter_class})

@mcp.tool()
async def delete_actor(actor_name: str, confirm: bool = False) -> dict:
    if not confirm: return {"warning": f"About to delete '{actor_name}'. Set confirm=true."}
    return await send("delete_actor", {"actor_name": actor_name})

@mcp.tool()
async def screenshot(width: int = 1920, height: int = 1080, quality: int = 85, fmt: str = "jpeg") -> str:
    r = await send("screenshot", {"width":width,"height":height,"quality":quality,"format":fmt})
    return str(r)

@mcp.tool()
async def scan_reflection() -> dict:
    return await send("scan_reflection", {})

@mcp.tool()
async def bp_create(path: str, parent_class: str = "Actor") -> dict:
    return await send("bp_create", {"path":path,"parent_class":parent_class})

@mcp.tool()
async def bp_list(path: str = "/Game/Blueprints") -> dict:
    return await send("bp_list", {"path":path})

@mcp.tool()
async def bp_open(path: str) -> dict:
    return await send("bp_open", {"path":path})

@mcp.tool()
async def bp_compile(path: str) -> dict:
    return await send("bp_compile", {"path":path})

@mcp.tool()
async def bp_create_node(path: str, graph: str, node_class: str, x: float = 0, y: float = 0, defaults: dict = None) -> dict:
    return await send("bp_create_node", {"path":path,"graph":graph,"node_class":node_class,"x":x,"y":y,"defaults":defaults or {}})

@mcp.tool()
async def bp_list_nodes(path: str, graph: str = "EventGraph") -> dict:
    return await send("bp_list_nodes", {"path":path,"graph":graph})

@mcp.tool()
async def bp_connect_pins(path: str, graph: str, src_node: str, src_pin: str, dst_node: str, dst_pin: str) -> dict:
    return await send("bp_connect_pins", {"path":path,"graph":graph,"src_node":src_node,"src_pin":src_pin,"dst_node":dst_node,"dst_pin":dst_pin})

@mcp.tool()
async def bp_create_variable(path: str, name: str, type: str, is_array: bool = False) -> dict:
    return await send("bp_create_variable", {"path":path,"name":name,"type":type,"is_array":is_array})

@mcp.tool()
async def bp_add_component(path: str, component_class: str, variable_name: str) -> dict:
    return await send("bp_add_component", {"path":path,"component_class":component_class,"variable_name":variable_name})

@mcp.tool()
async def bp_create_function(path: str, name: str, inputs: list = None, outputs: list = None) -> dict:
    return await send("bp_create_function", {"path":path,"name":name,"inputs":inputs or [],"outputs":outputs or []})

@mcp.tool()
async def create_material(asset_path: str, material_type: str = "opaque", base_color_hex: str = "FFFFFFFF", opacity: float = 1.0, roughness: float = 0.5, metallic: float = 0.0, specular: float = 0.5, emissive_strength: float = 0.0, emissive_hex: str = "000000FF") -> dict:
    return await send("create_material", {"asset_path":asset_path,"material_type":material_type,"base_color_hex":base_color_hex,"opacity":opacity,"roughness":roughness,"metallic":metallic,"specular":specular,"emissive_strength":emissive_strength,"emissive_hex":emissive_hex})

@mcp.tool()
async def modify_actor_property(actor_name: str, property_name: str, value) -> dict:
    return await send("modify_actor_property", {"actor_name":actor_name,"property_name":property_name,"value":value})

@mcp.tool()
async def set_viewport_camera(location: dict, rotation: dict = None) -> dict:
    return await send("set_viewport_camera", {"location":location,"rotation":rotation or {"pitch":0,"yaw":0,"roll":0}})

@mcp.tool()
async def bp_list_variables(path: str) -> dict:
    return await send("bp_list_variables", {"path":path})

@mcp.tool()
async def bp_remove_variable(path: str, name: str) -> dict:
    return await send("bp_remove_variable", {"path":path,"name":name})

@mcp.tool()
async def bp_set_variable_default(path: str, name: str, value: str) -> dict:
    return await send("bp_set_variable_default", {"path":path,"name":name,"value":value})

@mcp.tool()
async def bp_remove_node(path: str, graph: str, node_id: str) -> dict:
    return await send("bp_remove_node", {"path":path,"graph":graph,"node_id":node_id})

@mcp.tool()
async def bp_disconnect_pin(path: str, graph: str, node_id: str, pin: str) -> dict:
    return await send("bp_disconnect_pin", {"path":path,"graph":graph,"node_id":node_id,"pin":pin})

@mcp.tool()
async def bp_set_pin_default(path: str, graph: str, node_id: str, pin: str, value: str) -> dict:
    return await send("bp_set_pin_default", {"path":path,"graph":graph,"node_id":node_id,"pin":pin,"value":value})

@mcp.tool()
async def bp_list_functions(path: str) -> dict:
    return await send("bp_list_functions", {"path":path})

@mcp.tool()
async def bp_remove_function(path: str, name: str) -> dict:
    return await send("bp_remove_function", {"path":path,"name":name})

@mcp.tool()
async def bp_list_graphs(path: str) -> dict:
    return await send("bp_list_graphs", {"path":path})

@mcp.tool()
async def bp_list_components(path: str) -> dict:
    return await send("bp_list_components", {"path":path})

@mcp.tool()
async def bp_remove_component(path: str, name: str) -> dict:
    return await send("bp_remove_component", {"path":path,"name":name})

@mcp.tool()
async def bp_list_interfaces(path: str) -> dict:
    return await send("bp_list_interfaces", {"path":path})

@mcp.tool()
async def bp_add_interface(path: str, interface_class: str) -> dict:
    return await send("bp_add_interface", {"path":path,"interface_class":interface_class})

@mcp.tool()
async def bp_remove_interface(path: str, interface_class: str) -> dict:
    return await send("bp_remove_interface", {"path":path,"interface_class":interface_class})

@mcp.tool()
async def bp_list_macros(path: str) -> dict:
    return await send("bp_list_macros", {"path":path})

@mcp.tool()
async def bp_create_macro(path: str, name: str) -> dict:
    return await send("bp_create_macro", {"path":path,"name":name})

@mcp.tool()
async def bp_remove_macro(path: str, name: str) -> dict:
    return await send("bp_remove_macro", {"path":path,"name":name})

@mcp.tool()
async def bp_add_event_dispatcher(path: str, name: str) -> dict:
    return await send("bp_add_event_dispatcher", {"path":path,"name":name})

@mcp.tool()
async def bp_remove_event_dispatcher(path: str, name: str) -> dict:
    return await send("bp_remove_event_dispatcher", {"path":path,"name":name})

@mcp.tool()
async def bp_save(path: str) -> dict:
    return await send("bp_save", {"path":path})

@mcp.tool()
async def bp_list_node_classes() -> dict:
    return await send("bp_list_node_classes", {})

@mcp.tool()
async def add_action_mapping(action_name: str, key: str) -> dict:
    return await send("add_action_mapping", {"action_name":action_name,"key":key})

@mcp.tool()
async def add_axis_mapping(axis_name: str, key: str, scale: float = 1.0) -> dict:
    return await send("add_axis_mapping", {"axis_name":axis_name,"key":key,"scale":scale})

@mcp.tool()
async def enable_physics(actor_name: str, enable: bool = True) -> dict:
    return await send("enable_physics", {"actor_name":actor_name,"enable":enable})

@mcp.tool()
async def set_mass(actor_name: str, mass: float) -> dict:
    return await send("set_mass", {"actor_name":actor_name,"mass":mass})

@mcp.tool()
async def play_sound(actor_name: str, sound_path: str) -> dict:
    return await send("play_sound", {"actor_name":actor_name,"sound_path":sound_path})

@mcp.tool()
async def find_path(actor_name: str, x: float, y: float, z: float) -> dict:
    return await send("find_path", {"actor_name":actor_name,"x":x,"y":y,"z":z})

@mcp.tool()
async def apply_force(actor_name: str, x: float, y: float, z: float) -> dict:
    """对Actor施加力"""
    return await send("apply_force", {"actor_name":actor_name,"x":x,"y":y,"z":z})

@mcp.tool()
async def stop_sound(actor_name: str) -> dict:
    """停止Actor上的声音"""
    return await send("stop_sound", {"actor_name":actor_name})

@mcp.tool()
async def create_niagara(path: str, name: str) -> dict:
    return await send("create_niagara", {"path":path,"name":name})

@mcp.tool()
async def create_sequence(path: str, name: str) -> dict:
    return await send("create_sequence", {"path":path,"name":name})

@mcp.tool()
async def add_actor_to_sequencer(seq_path: str, actor_name: str) -> dict:
    """Sequencer添加Actor到轨道"""
    return await send("add_actor_to_sequencer", {"seq_path": seq_path, "actor_name": actor_name})

@mcp.tool()
# PCG disabled - plugin not available in project
# async def create_pcg(path: str, name: str) -> dict:
#     return await send("create_pcg", {"path":path,"name":name})

@mcp.tool()
async def copy_asset(source_path: str, dest_path: str) -> dict:
    return await send("copy_asset", {"source_path":source_path,"dest_path":dest_path})

@mcp.tool()
async def delete_asset(path: str) -> dict:
    return await send("delete_asset", {"path":path})

@mcp.tool()
async def list_assets(path: str = "/Game", class_name: str = "") -> dict:
    return await send("list_assets", {"path":path,"class_name":class_name})

async def ensure_connected():
    global _tcp_reader, _tcp_writer
    if _tcp_writer:
        try:
            _tcp_writer.write(b'\n'); await _tcp_writer.drain()
        except Exception:
            _tcp_reader = None; _tcp_writer = None
            return False
        return True
    try:
        _tcp_reader, _tcp_writer = await asyncio.wait_for(asyncio.open_connection('127.0.0.1', 9876), timeout=5)
        _tcp_writer.write(b'{"type":"mcp"}\n'); await _tcp_writer.drain()
        await _tcp_reader.readline()
        asyncio.get_event_loop().create_task(_tcp_read())
        return True
    except Exception:
            logger.debug("Bridge not available"); return False

async def _tcp_read():
    global _tcp_reader, _tcp_writer, _pending
    try:
        while _tcp_reader:
            line = await _tcp_reader.readline()
            if not line:
                break
            try:
                msg = json.loads(line.decode("utf-8"))
            except json.JSONDecodeError:
                continue
            rid = str(msg.get("id", ""))
            fut = _pending.pop(rid, None)
            if not fut or fut.done():
                continue
            if msg.get("error"):
                fut.set_result({
                    "error": msg.get("error"),
                    "error_code": msg.get("error_code", -99),
                })
            else:
                fut.set_result(msg.get("result", msg))
    except (ConnectionResetError, asyncio.IncompleteReadError, BrokenPipeError):
        pass
    finally:
        for fut in _pending.values():
            if not fut.done():
                fut.set_result({"error": "Bridge disconnected", "error_code": -5})
        _pending.clear()
        _tcp_reader = None
        _tcp_writer = None

async def send(method, params=None):
    global _pending
    if not await ensure_connected(): return {"error": "Bridge not running", "error_code": -5}
    async with _tcp_lock:
        rid = str(uuid.uuid4())
        f = asyncio.get_event_loop().create_future()
        _pending[rid] = f
        _tcp_writer.write((json.dumps({"id":rid,"method":method,"params":params or {}})+"\n").encode())
        await _tcp_writer.drain()
    try: return await asyncio.wait_for(f, timeout=30)
    except asyncio.TimeoutError:
        _pending.pop(rid,None); return {"error":"Timeout","error_code":-4}

@mcp.tool()
async def bt_create(path: str, name: str) -> str:
    """创建 BehaviorTree 资产"""
    return await send("bt_create", {"path": path, "name": name})

@mcp.tool()
async def bt_add_composite(path: str, parent_id: str, composite_type: str) -> str:
    """BT添加Select/Sequ/SimpleParallel节点"""
    return await send("bt_add_composite", {"path": path, "parent_id": parent_id, "composite_type": composite_type})

@mcp.tool()
async def bt_add_task(path: str, parent_id: str, task_class: str) -> str:
    """BT添加Task节点"""
    return await send("bt_add_task", {"path": path, "parent_id": parent_id, "task_class": task_class})

@mcp.tool()
async def bt_add_decorator(path: str, node_id: str, decorator_class: str) -> str:
    """BT节点添加Decorator"""
    return await send("bt_add_decorator", {"path": path, "node_id": node_id, "decorator_class": decorator_class})

@mcp.tool()
async def bt_add_service(path: str, composite_id: str, service_class: str) -> str:
    """BT复合节点添加Service"""
    return await send("bt_add_service", {"path": path, "composite_id": composite_id, "service_class": service_class})

@mcp.tool()
async def create_blackboard(path: str, name: str) -> str:
    """创建Blackboard资产"""
    return await send("create_blackboard", {"path": path, "name": name})

@mcp.tool()
async def add_bb_key(path: str, key_name: str, key_type: str) -> str:
    """Blackboard添加Key(Bool/Int/Float/String/Vector等)"""
    return await send("add_bb_key", {"path": path, "key_name": key_name, "key_type": key_type})

@mcp.tool()
async def anim_add_state(path: str, state_name: str, x: float, y: float) -> str:
    """AnimBP动画图添加状态节点"""
    return await send("anim_add_state", {"path": path, "state_name": state_name, "x": x, "y": y})

@mcp.tool()
async def anim_add_transition(path: str, from_state: str, to_state: str, condition: str = "") -> str:
    """AnimBP添加状态转换"""
    return await send("anim_add_transition", {"path": path, "from_state": from_state, "to_state": to_state, "condition": condition})

@mcp.tool()
async def anim_set_graph_node(path: str, node_class: str, x: float, y: float) -> str:
    """AnimBP添加通用动画图节点"""
    return await send("anim_set_graph_node", {"path": path, "node_class": node_class, "x": x, "y": y})

@mcp.tool()
async def create_widget(path: str, name: str) -> str:
    """创建WidgetBlueprint资产"""
    return await send("create_widget", {"path": path, "name": name})

@mcp.tool()
async def add_widget_to_canvas(path: str, widget_class: str, name: str) -> str:
    """WidgetBlueprint Canvas中添加控件"""
    return await send("add_widget_to_canvas", {"path": path, "widget_class": widget_class, "name": name})

@mcp.tool()
async def widget_set_text(path: str, widget: str, text: str) -> str:
    """设置Widget文本(TextBlock/Button)"""
    return await send("widget_set_text", {"path": path, "widget": widget, "text": text})

@mcp.tool()
async def widget_set_font(path: str, widget: str, size: int = 0, color: str = "") -> str:
    """设置TextBlock字体大小和颜色(RRGGBBAA)"""
    return await send("widget_set_font", {"path": path, "widget": widget, "size": size, "color": color})

@mcp.tool()
async def widget_list_tree(path: str) -> str:
    """列出WidgetBlueprint控件树"""
    return await send("widget_list_tree", {"path": path})

@mcp.tool()
async def widget_set_position(path: str, widget: str, x: float, y: float, w: float, h: float) -> str:
    """设置控件在Canvas中的位置和尺寸"""
    return await send("widget_set_position", {"path": path, "widget": widget, "x": x, "y": y, "w": w, "h": h})

@mcp.tool()
async def widget_add_to_viewport(path: str) -> str:
    """编译WidgetBlueprint并添加到编辑器视口"""
    return await send("widget_add_to_viewport", {"path": path})

@mcp.tool()
async def widget_set_visibility(path: str, widget: str, visible: str = "visible") -> str:
    """设置控件可见性(visible/collapsed/hidden/hit_test_invisible/self_hit_test_invisible)"""
    return await send("widget_set_visibility", {"path": path, "widget": widget, "visible": visible})

@mcp.tool()
async def add_emitter(path: str, emitter_path: str) -> str:
    """Niagara系统添加Emitter"""
    return await send("add_emitter", {"path": path, "emitter_path": emitter_path})

@mcp.tool()
async def set_niagara_param(path: str, param_name: str, value: float) -> str:
    """设置Niagara参数值"""
    return await send("set_niagara_param", {"path": path, "param_name": param_name, "value": value})

@mcp.tool()
async def add_transform_track(seq_path: str, actor_name: str) -> str:
    """Sequencer为Actor添加TransformTrack"""
    return await send("add_transform_track", {"seq_path": seq_path, "actor_name": actor_name})

@mcp.tool()
async def set_keyframe(seq_path: str, actor_name: str, time: float, x: float, y: float, z: float) -> str:
    """Sequencer设置Transform关键帧"""
    return await send("set_keyframe", {"seq_path": seq_path, "actor_name": actor_name, "time": time, "x": x, "y": y, "z": z})

@mcp.tool()
async def create_datatable(path: str, name: str, row_struct: str) -> str:
    """创建DataTable资产"""
    return await send("create_datatable", {"path": path, "name": name, "row_struct": row_struct})

@mcp.tool()
async def add_datatable_row(path: str, row_name: str, row_data: dict) -> str:
    """DataTable添加行数据"""
    return await send("add_datatable_row", {"path": path, "row_name": row_name, "row_data": row_data})

@mcp.tool()
async def get_datatable_row(path: str, row_name: str) -> str:
    """DataTable查询行数据"""
    return await send("get_datatable_row", {"path": path, "row_name": row_name})

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9876)
    parser.add_argument("--log-level", default="INFO")
    args = parser.parse_args()
    logging.basicConfig(level=getattr(logging, args.log_level), format="[%(asctime)s] %(message)s", datefmt="%H:%M:%S")
    logger.info("MCP Server starting...")
    mcp.run(transport="stdio")

if __name__ == "__main__":
    main()
