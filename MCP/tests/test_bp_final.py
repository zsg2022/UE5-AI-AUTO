import asyncio,json,uuid,time
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

async def main():
    uid=str(uuid.uuid4())[:8];P='/Game/Blueprints/BP_Final_'+uid
    print('=== Final Blueprint Test ===')
    r,e=await cmd('bp_create',{'path':P,'parent_class':'Actor'});print('1.Create:',r.get('status',e) if r else e)
    r,e=await cmd('bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_Event','x':0,'y':0,'defaults':{'event':'BeginPlay'}});print('2.BeginPlay:',r.get('id','?')[:16] if r else 'FAIL')
    r,e=await cmd('bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_CallFunction','x':500,'y':0,'defaults':{'function':'PrintString'}});print('3.PrintString:',r.get('id','?')[:16] if r else 'FAIL')
    r,e=await cmd('bp_list_nodes',{'path':P,'graph':'EventGraph'})
    ns=r.get('nodes',[]) if r else [];print('4.List:%d nodes'%len(ns))
    bid=psid=None
    for n in ns:
        t=n.get('title','')
        if 'BeginPlay' in t or 'ReceiveBeginPlay' in t:bid=n['id']
        if 'Print' in t:psid=n['id']
    if bid and psid:
        r,e=await cmd('bp_connect_pins',{'path':P,'graph':'EventGraph','src_node':bid,'src_pin':'then','dst_node':psid,'dst_pin':'execute'});print('5.Connect:',r.get('status',e) if r else e)
        r,e=await cmd('bp_set_pin_default',{'path':P,'graph':'EventGraph','node_id':psid,'pin':'InString','value':'AI Works!'});print('6.Default:',r.get('status',e) if r else e)
    else:print('5/6: SKIP - nodes not found')
    r,e=await cmd('bp_list_variables',{'path':P});print('7.Vars:',r.get('count',0) if r else e)
    r,e=await cmd('bp_create_function',{'path':P,'name':'TestFunc','inputs':[],'outputs':[]});print('8.Func:',r.get('status',e) if r else e)
    r,e=await cmd('bp_list_functions',{'path':P});print('9.Funcs:',r.get('count',0) if r else e)
    r,e=await cmd('bp_list_graphs',{'path':P});print('10.Graphs:',r.get('count',0) if r else e)
    r,e=await cmd('bp_compile',{'path':P});print('11.Compile:',r.get('status',e) if r else e)
    r,e=await cmd('bp_list_node_classes',{});print('12.Types:',r.get('count',0) if r else e)
    r,e=await cmd('bp_list',{'path':'/Game/Blueprints'});print('13.List BPs:',r.get('count',0) if r else e)
    r,e=await cmd('bp_open',{'path':P});print('14.Open:',r.get('name','?') if r else e)
    if bid:
        r,e=await cmd('bp_remove_node',{'path':P,'graph':'EventGraph','node_id':bid});print('15.Remove:',r.get('status',e) if r else e)
    print('=== DONE ===')
asyncio.run(main())
