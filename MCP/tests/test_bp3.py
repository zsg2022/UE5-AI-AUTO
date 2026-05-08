import asyncio,json,uuid,time
async def cmd(m,p={}):
    r,w=await asyncio.open_connection('127.0.0.1',9876)
    w.write(b'{"type":"mcp"}\n'); await w.drain()
    await asyncio.wait_for(r.readline(),timeout=5)
    rid=str(uuid.uuid4())
    w.write((json.dumps({'id':rid,'method':m,'params':p})+'\n').encode()); await w.drain()
    buf=b''; deadline=time.time()+20
    while time.time()<deadline:
        try:
            chunk=await asyncio.wait_for(r.read(65536),timeout=3)
            buf+=chunk
            while b'\n' in buf:
                line,buf=buf.split(b'\n',1)
                line=line.replace(b'\r',b'').strip()
                if not line: continue
                try: resp=json.loads(line.decode())
                except: continue
                if resp.get('id')==rid:
                    w.close()
                    return resp.get('result',{}), resp.get('error')
        except asyncio.TimeoutError: continue
    w.close(); return {},'timeout'

async def t():
    uid=str(uuid.uuid4())[:8]
    P='/Game/Blueprints/BP_OK_'+uid
    print('=== Blueprint Full Workflow:',P,'===')
    r,e=await cmd('bp_create',{'path':P,'parent_class':'Actor'})
    print('1 Create:',r.get('status','?') if r else 'timeout')
    r,e=await cmd('bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_Event','x':0,'y':0,'defaults':{'event':'BeginPlay'}})
    bid=r.get('id','') if r else ''; print('2 BeginPlay:',bid[:16] if bid else 'FAIL')
    r,e=await cmd('bp_create_node',{'path':P,'graph':'EventGraph','node_class':'K2Node_CallFunction','x':500,'y':0,'defaults':{'function':'PrintString'}})
    pid=r.get('id','') if r else ''; print('3 PrintString:',pid[:16] if pid else 'FAIL')
    r,e=await cmd('bp_connect_pins',{'path':P,'graph':'EventGraph','src_node':bid,'src_pin':'then','dst_node':pid,'dst_pin':'execute'})
    print('4 Connect:',r.get('status','?') if r else 'timeout')
    r,e=await cmd('bp_set_pin_default',{'path':P,'graph':'EventGraph','node_id':pid,'pin':'InString','value':'Hello AI!'})
    print('5 Default:',r.get('status','?') if r else 'timeout')
    r,e=await cmd('bp_compile',{'path':P})
    print('6 Compile:',r.get('status','?') if r else 'timeout')
    r,e=await cmd('bp_list_node_classes',{})
    print('7 Types:',r.get('count','?') if r else 'timeout')
    print('=== DONE ===')
asyncio.run(t())
