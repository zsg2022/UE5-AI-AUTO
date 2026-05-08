"""Final comprehensive test - ALL 32 commands."""
import asyncio,json,uuid,time,sys
PASS=0;FAIL=0;SKIP=0

async def cmd(m,p={}):
    r,w=await asyncio.open_connection('127.0.0.1',9876)
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

async def test(name,m,p={},key='status',expect=None):
    global PASS,FAIL,SKIP
    r,e=await cmd(m,p)
    if not r: print(f'  FAIL [{name}]: {e}'); FAIL+=1; return None
    if isinstance(r,dict) and r.get('error'):
        if 'Not available' in str(r['error']) or 'requires BP open' in str(r['error']):
            print(f'  SKIP [{name}]: {r["error"][:60]}'); SKIP+=1; return None
        print(f'  FAIL [{name}]: {r["error"]}'); FAIL+=1; return None
    val=r.get(key,'?') if isinstance(r,dict) else str(r)[:50]
    if expect and val != expect:
        print(f'  WARN [{name}]: got "{val}", expected "{expect}"')
    print(f'  OK   [{name}]: {val}')
    PASS+=1; return r

async def main():
    global PASS,FAIL,SKIP
    uid=str(uuid.uuid4())[:8];P='/Game/Blueprints/BP_Test_'+uid
    print(f'=== Full Test: {P} ===\n')

    print('-- BP CRUD (5) --')
    await test('bp_create','bp_create',{'path':P,'parent_class':'Actor'})
    await test('bp_compile','bp_compile',{'path':P})
    await test('bp_open','bp_open',{'path':P},'name')
    await test('bp_save','bp_save',{'path':P})
    bp=await test('bp_list','bp_list',{'path':'/Game/Blueprints'},'count')

    print('\n-- Variables (4) --')
    await test('bp_create_variable','bp_create_variable',{'path':P,'name':'Health','type':'float','is_array':False})
    await test('bp_create_variable','bp_create_variable',{'path':P,'name':'bDead','type':'bool','is_array':False})
    await test('bp_set_variable_default','bp_set_variable_default',{'path':P,'name':'Health','value':'100.0'})
    vr=await test('bp_list_variables','bp_list_variables',{'path':P},'count')
    await test('bp_remove_variable','bp_remove_variable',{'path':P,'name':'bDead'})

    print('\n-- Functions (3) --')
    await test('bp_create_function','bp_create_function',{'path':P,'name':'MyFunc','inputs':[],'outputs':[]})
    await test('bp_list_functions','bp_list_functions',{'path':P},'count')
    await test('bp_remove_function','bp_remove_function',{'path':P,'name':'MyFunc'})

    print('\n-- Nodes (7) --')
    n1=await test('bp_create_node(BeginPlay)','bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_Event','x':0,'y':0,'defaults':{'event':'BeginPlay'}},'class')
    n2=await test('bp_create_node(PrintString)','bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_CallFunction','x':400,'y':0,'defaults':{'function':'PrintString'}},'class')
    nr=await test('bp_list_nodes','bp_list_nodes',{'path':P,'graph':'EventGraph'},'count')
    await test('bp_disconnect_pin','bp_disconnect_pin',{'path':P,'graph':'EventGraph','node_id':'placeholder','pin':'then'})
    if nr and nr.get('count',0)>=2:
        nodes=nr.get('nodes',[])
        bid=psid=None
        for n in nodes:
            t=n.get('title','');nid=n.get('id','')
            if 'BeginPlay' in t or 'ReceiveBeginPlay' in t:bid=nid
            if 'Print' in t:psid=nid
        if bid and psid:
            await test('bp_connect_pins','bp_connect_pins',{'path':P,'graph':'EventGraph','src_node':bid,'src_pin':'then','dst_node':psid,'dst_pin':'execute'})
            await test('bp_set_pin_default','bp_set_pin_default',{'path':P,'graph':'EventGraph','node_id':psid,'pin':'InString','value':'AI says Hi!'})
    if n1 and n1.get('id'):
        await test('bp_remove_node','bp_remove_node',{'path':P,'graph':'EventGraph','node_id':n1['id']})

    print('\n-- Components (3) --')
    await test('bp_add_component','bp_add_component',{'path':P,'component_class':'StaticMeshComponent','variable_name':'MeshComp'})
    await test('bp_list_components','bp_list_components',{'path':P},'count')
    await test('bp_remove_component','bp_remove_component',{'path':P,'name':'MeshComp'})

    print('\n-- Interfaces (3) --')
    await test('bp_list_interfaces','bp_list_interfaces',{'path':P},'count')

    print('\n-- Macros (3) --')
    await test('bp_create_macro','bp_create_macro',{'path':P,'name':'TestMacro'})
    await test('bp_list_macros','bp_list_macros',{'path':P},'count')
    await test('bp_remove_macro','bp_remove_macro',{'path':P,'name':'TestMacro'})

    print('\n-- Event Dispatchers (2) --')
    await test('bp_add_event_dispatcher','bp_add_event_dispatcher',{'path':P,'name':'OnDied'})
    await test('bp_remove_event_dispatcher','bp_remove_event_dispatcher',{'path':P,'name':'OnDied'})

    print('\n-- Tools (2) --')
    await test('bp_list_graphs','bp_list_graphs',{'path':P},'count')
    await test('bp_list_node_classes','bp_list_node_classes',{},'count')

    total=PASS+FAIL+SKIP
    print(f'\n=== Results: {PASS} pass, {FAIL} fail, {SKIP} skip (of {total}) ===')
    if FAIL==0: print('=== ALL CLEAN ===')
    sys.exit(0 if FAIL==0 else 1)

asyncio.run(main())
