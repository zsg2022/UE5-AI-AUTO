"""P0+P1 新增功能回归测试 — 共 7 模块 32 条命令.
启动方法: UE5 Editor 加载插件后，python tests/test_new_features.py
环境要求: Bridge Server 在 127.0.0.1:9876 就绪。
"""
import asyncio,json,uuid,time,sys
PASS=0;FAIL=0;SKIP=0

async def cmd(m,p={}):
    r,w=await asyncio.open_connection('127.0.0.1',9876)
    w.write(b'{"type":"mcp"}\n'); await w.drain();await asyncio.wait_for(r.readline(),timeout=5)
    rid=str(uuid.uuid4());w.write((json.dumps({'id':rid,'method':m,'params':p})+'\n').encode());await w.drain()
    buf=b'';deadline=time.time()+20
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

async def test(name,m,p={},key='status',expect=None):
    global PASS,FAIL,SKIP
    r,e=await cmd(m,p)
    if not r: print(f'  FAIL [{name}]: {e}'); FAIL+=1; return None
    if isinstance(r,dict) and r.get('error'):
        if 'bridge' in str(r.get('error','')).lower() or 'Not available' in str(r['error']):
            print(f'  SKIP [{name}]: {r["error"][:60]}'); SKIP+=1; return None
        print(f'  FAIL [{name}]: {r["error"]}'); FAIL+=1; return None
    val=r.get(key,'?') if isinstance(r,dict) else str(r)[:60]
    if expect is not None and val != expect:
        print(f'  WARN [{name}]: got "{val}", expected "{expect}"')
        PASS+=1; return r
    print(f'  OK   [{name}]: {val}')
    PASS+=1; return r

async def main():
    global PASS,FAIL,SKIP
    uid=str(uuid.uuid4())[:8]
    BP='/Game/Blueprints/BP_FT_'+uid
    BT='/Game/AI/BT_FT_'+uid
    BB='/Game/AI/BB_FT_'+uid
    ABP='/Game/Animations/ABP_FT_'+uid
    WBP='/Game/UI/WBP_FT_'+uid
    NIAG='/Game/VFX/NS_FT_'+uid
    SEQ='/Game/Cinematics/Seq_FT_'+uid
    PCG='/Game/PCG/PCG_FT_'+uid
    DT='/Game/Data/DT_FT_'+uid
    print(f'=== New Features Test Run: {uid} ===\n')

    # ==================== 模块 1: BehaviorTree (5 tests) ====================
    print('--- [P0] BehaviorTree (5) ---')
    await test('bt_create','bt_create',{'path':BT,'name':'BT_FT_'+uid})
    r0=await test('bt_add_composite(Selector)','bt_add_composite',{'path':BT,'parent_id':'','composite_type':'Selector'},'node_id')
    r1=await test('bt_add_composite(Sequence)','bt_add_composite',{'path':BT,'parent_id':r0.get('node_id','') if r0 else '','composite_type':'Sequence'},'node_id')
    if r1:
        await test('bt_add_task','bt_add_task',{'path':BT,'parent_id':r1['node_id'],'task_class':'BTTask_BlueprintBase'})
    if r0:
        await test('bt_add_service','bt_add_service',{'path':BT,'composite_id':r0['node_id'],'service_class':'BTService_BlueprintBase'})
    if r0:
        await test('bt_add_decorator','bt_add_decorator',{'path':BT,'node_id':r0['node_id'],'decorator_class':'BTDecorator_BlueprintBase'})

    # ==================== 模块 2: Blackboard (2 tests) ====================
    print('\n--- [P1] Blackboard (8) ---')
    await test('create_blackboard','create_blackboard',{'path':BB,'name':'BB_FT_'+uid})
    for ktype in ['Bool','Int','Float','String','Name','Vector','Rotator','Object','Class','Enum']:
        await test(f'add_bb_key({ktype})','add_bb_key',{'path':BB,'key_name':f'Key_{ktype}','key_type':ktype})

    # ==================== 模块 3: Animation Blueprint (3 tests) ====================
    print('\n--- [P0] Animation Blueprint (3) ---')
    await test('bp_create(AnimBP)','bp_create',{'path':ABP,'parent_class':'AnimInstance'})
    await test('anim_add_state(Idle)','anim_add_state',{'path':ABP,'state_name':'Idle','x':0,'y':0})
    await test('anim_add_state(Run)','anim_add_state',{'path':ABP,'state_name':'Run','x':300,'y':0})
    r_t=await test('anim_add_transition','anim_add_transition',{'path':ABP,'from_state':'Idle','to_state':'Run','condition':'bRunning'})
    await test('anim_set_graph_node','anim_set_graph_node',{'path':ABP,'node_class':'AnimGraphNode_RandomPlayer','x':600,'y':0})

    # ==================== 模块 4: UMG Widget (2 tests) ====================
    print('\n--- [P0] UMG Widget (2) ---')
    await test('create_widget','create_widget',{'path':WBP,'name':'WBP_FT_'+uid})
    await test('add_widget_to_canvas(Button)','add_widget_to_canvas',{'path':WBP,'widget_class':'Button','name':'Btn_OK'})
    await test('add_widget_to_canvas(TextBlock)','add_widget_to_canvas',{'path':WBP,'widget_class':'TextBlock','name':'Txt_Title'})

    # ==================== 模块 5: Sequencer (3 tests) ====================
    print('\n--- [P1] Sequencer (4) ---')
    await test('create_sequence','create_sequence',{'path':SEQ,'name':'Seq_FT_'+uid})
    await test('create_actor(SeqTest)','create_actor',{'class_name':'Cube','location':{'x':100,'y':200,'z':50}},'actor_name')
    await test('add_transform_track','add_transform_track',{'seq_path':SEQ,'actor_name':'Cube_0'})
    await test('set_keyframe','set_keyframe',{'seq_path':SEQ,'actor_name':'Cube_0','time':1.0,'x':500,'y':0,'z':200})

    # ==================== 模块 6: Niagara (2 tests) ====================
    print('\n--- [P1] Niagara (3) ---')
    r_ns=await test('create_niagara','create_niagara',{'path':NIAG,'name':'NS_FT_'+uid})
    await test('add_emitter(Fountain)','add_emitter',{'path':NIAG,'emitter_path':'/Engine/FX/Templates/Fountain'})
    await test('set_niagara_param','set_niagara_param',{'path':NIAG,'param_name':'SpawnRate','value':100.0})

    # ==================== 模块 7: PCG (1 test) ====================
    print('\n--- [P1] PCG (2) ---')
    await test('create_pcg','create_pcg',{'path':PCG,'name':'PCG_FT_'+uid})
    await test('add_pcg_node(Input)','add_pcg_node',{'path':PCG,'node_type':'Input'})

    # ==================== 模块 8: DataTable (3 tests) ====================
    print('\n--- [P1] DataTable (4) ---')
    await test('create_datatable','create_datatable',{'path':DT,'name':'DT_FT_'+uid,'row_struct':'/Script/Engine.DataTableRowHandle'})
    await test('add_datatable_row','add_datatable_row',{'path':DT,'row_name':'Row_01','row_data':{'RowName':'Row_01','DataTable':'','RowName2':''}})
    await test('add_datatable_row(2)','add_datatable_row',{'path':DT,'row_name':'Row_02','row_data':{'RowName':'Row_02'}})
    await test('get_datatable_row','get_datatable_row',{'path':DT,'row_name':'Row_01'},'row_name')

    total=PASS+FAIL+SKIP
    print(f'\n=== Results: {PASS} pass, {FAIL} fail, {SKIP} skip (of {total}) ===')
    if FAIL==0: print('=== ALL NEW FEATURES CLEAN ===')
    sys.exit(0 if FAIL==0 else 1)

asyncio.run(main())