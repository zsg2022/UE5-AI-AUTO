"""Complete blueprint test - ALL capabilities."""
import asyncio,json,uuid,time
PASS=0;FAIL=0

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

async def check(name,method,params={},expect_field='status'):
    global PASS,FAIL
    r,e=await cmd(method,params)
    if r:
        val=r.get(expect_field,'?') if isinstance(r,dict) else str(r)[:50]
        print('  [OK] %s: %s'%(name,val));PASS+=1
    else:
        print('  [FAIL] %s: %s'%(name,e));FAIL+=1
    return r

async def main():
    global PASS,FAIL
    uid=str(uuid.uuid4())[:8];P='/Game/Blueprints/BP_All_'+uid
    print('=== Full Blueprint Test: %s ===\n'%P)

    print('-- CRUD --')
    await check('Create','bp_create',{'path':P,'parent_class':'Actor'})
    await check('Open','bp_open',{'path':P},'name')
    await check('Compile','bp_compile',{'path':P})

    print('\n-- Variables --')
    await check('Create float','bp_create_variable',{'path':P,'name':'Health','type':'float','is_array':False})
    await check('Create bool','bp_create_variable',{'path':P,'name':'bAlive','type':'bool','is_array':False})
    await check('Create string','bp_create_variable',{'path':P,'name':'Name','type':'string','is_array':False})
    await check('Set default','bp_set_variable_default',{'path':P,'name':'Health','value':'100.0'},None)
    await check('List vars','bp_list_variables',{'path':P},'count')
    await check('Remove var','bp_remove_variable',{'path':P,'name':'bAlive'})

    print('\n-- Nodes --')
    await check('Create BeginPlay','bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_Event','x':0,'y':0,'defaults':{'event':'BeginPlay'}},'id')
    await check('Create PrintString','bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_CallFunction','x':400,'y':0,'defaults':{'function':'PrintString'}},'id')
    await check('Create Health Get','bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_VariableGet','x':0,'y':200,'defaults':{'variable':'Health'}},'id')

    # List nodes and get IDs for connect
    r=await check('List nodes','bp_list_nodes',{'path':P,'graph':'EventGraph'},'count')
    nodes=r.get('nodes',[]) if r else []
    bid=psid=hgid=None
    for n in nodes:
        t=n.get('title','');nid=n.get('id','')
        if 'BeginPlay' in t or 'ReceiveBeginPlay' in t:bid=nid
        if 'Print' in t:psid=nid
        if 'Health' in t and not hgid:hgid=nid

    if bid and psid:
        await check('Connect then->exec','bp_connect_pins',{'path':P,'graph':'EventGraph','src_node':bid,'src_pin':'then','dst_node':psid,'dst_pin':'execute'})
        await check('Set InString','bp_set_pin_default',{'path':P,'graph':'EventGraph','node_id':psid,'pin':'InString','value':'AI Worked!'})

    print('\n-- Functions --')
    await check('Create func','bp_create_function',{'path':P,'name':'TestFunc','inputs':[],'outputs':[]})
    await check('List funcs','bp_list_functions',{'path':P},'count')
    await check('Remove func','bp_remove_function',{'path':P,'name':'TestFunc'})

    print('\n-- Components --')
    await check('Add StaticMesh','bp_add_component',{'path':P,'component_class':'StaticMeshComponent','variable_name':'MyMesh'})
    await check('List comps','bp_list_components',{'path':P},'count')
    await check('Remove comp','bp_remove_component',{'path':P,'name':'MyMesh'})

    print('\n-- Interfaces --')
    await check('List ifaces','bp_list_interfaces',{'path':P},'count')

    print('\n-- Macros --')
    await check('List macros','bp_list_macros',{'path':P},'count')

    print('\n-- Event Dispatchers --')
    await check('Add dispatcher','bp_add_event_dispatcher',{'path':P,'name':'OnDamage'})

    print('\n-- Tools --')
    await check('Node types','bp_list_node_classes',{},'count')
    await check('Save','bp_save',{'path':P})

    print('\n=== Results: %d pass, %d fail ==='%(PASS,FAIL))
asyncio.run(main())
