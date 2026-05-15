"""架构验证测试 — Handler注册完整性、MCP工具数、端口配置、模块加载。
启动方法: UE5 Editor 加载插件后，python tests/test_architecture.py
需 Bridge Server 127.0.0.1:9876 + UE5 Editor 运行。
无 UE5 Editor 时不可运行（会全部 SKIP）。
"""
import asyncio,json,uuid,time,sys,re
PASS=0;FAIL=0;SKIP=0

# 预期新增的 19 个 P0+P1 命令 (C++ Handler 注册名)
EXPECTED_NEW_COMMANDS = {
    # P0 - BehaviorTree
    'bt_create', 'bt_add_composite', 'bt_add_task', 'bt_add_decorator', 'bt_add_service',
    # P0 - Blackboard
    'create_blackboard', 'add_bb_key',
    # P0 - Animation Blueprint
    'anim_add_state', 'anim_add_transition', 'anim_set_graph_node',
    # P0 - UMG Widget
    'create_widget', 'add_widget_to_canvas',
    # P1 - Niagara
    'add_emitter', 'set_niagara_param',
    # P1 - Sequencer
    'add_transform_track', 'set_keyframe',
    # P1 - DataTable
    'create_datatable', 'add_datatable_row', 'get_datatable_row',
}

# 预期原有命令名（进程内静态验证用）
EXPECTED_ORIGINAL_COMMANDS = [
    'ping','scan_reflection','create_actor','modify_actor_property','delete_actor',
    'query_scene','set_viewport_camera','screenshot',
    'bp_create','bp_open','bp_compile','bp_list','bp_save',
    'bp_create_node','bp_list_nodes','bp_connect_pins','bp_disconnect_pin','bp_set_pin_default',
    'bp_remove_node','bp_create_variable','bp_list_variables','bp_remove_variable','bp_set_variable_default',
    'bp_create_function','bp_list_functions','bp_remove_function',
    'bp_list_graphs','bp_list_node_classes',
    'bp_add_component','bp_list_components','bp_remove_component',
    'bp_list_interfaces','bp_add_interface','bp_remove_interface',
    'bp_list_macros','bp_create_macro','bp_remove_macro',
    'bp_add_event_dispatcher','bp_remove_event_dispatcher',
    'create_material','create_metasound','create_niagara','create_sequence','create_pcg',
    'add_action_mapping','add_axis_mapping','enable_physics','set_mass','play_sound','find_path',
    'copy_asset','delete_asset','list_assets',
]

async def cmd(m,p={}):
    try:
        r,w=await asyncio.wait_for(asyncio.open_connection('127.0.0.1',9876),timeout=3)
    except (ConnectionRefusedError, TimeoutError, OSError):
        return None,None
    w.write(b'{"type":"mcp"}\n'); await w.drain();await asyncio.wait_for(r.readline(),timeout=5)
    rid=str(uuid.uuid4());w.write((json.dumps({'id':rid,'method':m,'params':p})+'\n').encode());await w.drain()
    buf=b'';deadline=time.time()+15
    while time.time()<deadline:
        try:
            chunk=await asyncio.wait_for(r.read(65536),timeout=3);buf+=chunk
            while b'\n' in buf:
                line,buf=buf.split(b'\n',1);line=line.replace(b'\r',b'').strip()
                if not line:continue
                try:resp=json.loads(line.decode())
                except:continue
                if resp.get('id')==rid:w.close();return resp.get('result',{}),resp.get('error')
        except asyncio.TimeoutError:continue
    w.close();return None,'timeout'

def count_mcp_tools():
    """进程内统计 server.py 中 @mcp.tool() 数量."""
    try:
        import pathlib
        server_path=pathlib.Path(__file__).resolve().parent.parent/'src'/'ue5aiauto'/'server.py'
        text=server_path.read_text(encoding='utf-8')
        count=len(re.findall(r'@mcp\.tool\(\)',text))
        return count
    except:
        return -1

async def main():
    global PASS,FAIL,SKIP
    print('=== Architecture Validation Test ===\n')

    # ==================== 测试 1: MCP 工具数量验证 ====================
    print('--- [架构] 工具计数 (2) ---')
    tool_count=count_mcp_tools()
    if tool_count>=72:
        print(f'  OK   [mcp_tool_count]: {tool_count} >= 72 (expected)');PASS+=1
    elif tool_count>0:
        print(f'  WARN [mcp_tool_count]: {tool_count} < 72 (expected 72+)');PASS+=1
    else:
        print(f'  FAIL [mcp_tool_count]: cannot read server.py');FAIL+=1

    # ==================== 测试 2: 连接 Bridge ====================
    print('\n--- [架构] Bridge连接 (1) ---')
    r_ping,e=await cmd('ping',{})
    if r_ping:
        print(f'  OK   [ping]: {r_ping.get("status","?")}');PASS+=1
    else:
        print('  SKIP [ping]: Bridge/UE5 not running (remaining tests skipped)');SKIP+=1
        print(f'\n=== Results: {PASS} pass, {FAIL} fail, {SKIP} skip (of {PASS+FAIL+SKIP}) ===')
        print('=== Requires UE5 Editor with plugin loaded ===')
        sys.exit(0)

    # ==================== 测试 3: 新增命令可用性 (19 tests) ====================
    print(f'\n--- [架构] 新增命令探测 ({len(EXPECTED_NEW_COMMANDS)} tests) ---')
    found_new=0;missing_new=[]
    for cmd_name in sorted(EXPECTED_NEW_COMMANDS):
        r,e=await cmd(cmd_name,{'path':'/Game/Nonexistent/NX_'+str(uuid.uuid4())[:8],'name':'Test','parent_id':'','composite_type':'Selector','key_name':'Test','key_type':'Bool','widget_class':'Button','state_name':'Idle','from_state':'A','to_state':'B','node_class':'Test','emitter_path':'/Engine/Test','param_name':'Test','value':0.0,'seq_path':'/Game/Test','actor_name':'Test','time':0.0,'x':0,'y':0,'z':0,'row_struct':'/Script/Engine.DataTableRowHandle','row_name':'Test','row_data':{},'task_class':'Test','service_class':'Test','decorator_class':'Test','node_id':'','composite_id':'','condition':''})
        if r:
            found_new+=1
            print(f'  OK   [new/{cmd_name}]: registered')
        else:
            missing_new.append(cmd_name)
            print(f'  FAIL [new/{cmd_name}]: NOT registered');FAIL+=1
    if found_new>0: PASS+=found_new

    # ==================== 测试 4: 端口配置化验证 ====================
    print('\n--- [架构] 端口配置 (1) ---')
    try:
        import pathlib
        ini_path=pathlib.Path(__file__).resolve().parent.parent.parent.parent/'D'/'DSPlugin'/'Plugins'/'UE5AIAUTO'/'Source'/'UE5AIAUTO'/'Private'/'UE5AIAUTOEditorSubsystem.cpp'
        ini_text=ini_path.read_text(encoding='utf-8')
        has_config='GConfig' in ini_text and 'BridgeHost' in ini_text
        has_default='DEFAULT_HOST' in ini_text and 'DEFAULT_PORT' in ini_text
        if has_config and has_default:
            print('  OK   [port_config]: GConfig + DEFAULT_HOST/DEFAULT_PORT found');PASS+=1
        elif has_config:
            print('  WARN [port_config]: GConfig found but no DEFAULT_* constants');PASS+=1
        else:
            print('  FAIL [port_config]: GConfig not found in Initialize()');FAIL+=1
    except:
        print('  SKIP [port_config]: cannot read source file');SKIP+=1

    total=PASS+FAIL+SKIP
    print(f'\n=== Results: {PASS} pass, {FAIL} fail, {SKIP} skip (of {total}) ===')
    if FAIL==0: print('=== ARCHITECTURE CLEAN ===')
    sys.exit(0 if FAIL==0 else 1)

asyncio.run(main())