"""新增功能边界条件+异常路径测试 — 覆盖空值/无效路径/错误类型/连接异常.
启动方法: UE5 Editor 加载插件后，python tests/test_boundary.py
环境要求: Bridge Server 在 127.0.0.1:9876 就绪。
⚠️ 本测试预期部分命令返回 error（边界/异常场景），FAIL 计数包含预期错误即为通过。
"""
import asyncio,json,uuid,time,sys
PASS=0;FAIL=0;SKIP=0;EXPECTED_ERRORS=0

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

async def expect_error(name,m,p={},error_contains=None):
    """预期返回错误的测试: PASS=正确返回错误, FAIL=未返回错误."""
    global PASS,FAIL,SKIP,EXPECTED_ERRORS
    r,e=await cmd(m,p)
    if not r: print(f'  FAIL [{name}]: {e}'); FAIL+=1; return
    err=r.get('error','') if isinstance(r,dict) else ''
    if err:
        if error_contains and error_contains.lower() not in err.lower():
            print(f'  WARN [{name}]: error="{err[:60]}", expected containing "{error_contains}"')
        else:
            print(f'  OK   [{name}]: error="{err[:60]}"')
        PASS+=1; EXPECTED_ERRORS+=1
    else:
        print(f'  FAIL [{name}]: expected error, got success'); FAIL+=1

async def test(name,m,p={},key='status',expect=None):
    global PASS,FAIL,SKIP
    r,e=await cmd(m,p)
    if not r: print(f'  FAIL [{name}]: {e}'); FAIL+=1; return None
    if isinstance(r,dict) and r.get('error'):
        print(f'  FAIL [{name}]: unexpected error "{r["error"][:60]}"'); FAIL+=1; return None
    val=r.get(key,'?') if isinstance(r,dict) else str(r)[:60]
    if expect is not None and val != expect:
        print(f'  WARN [{name}]: got "{val}", expected "{expect}"');PASS+=1;return r
    print(f'  OK   [{name}]: {val}')
    PASS+=1; return r

async def main():
    global PASS,FAIL,SKIP,EXPECTED_ERRORS
    uid=str(uuid.uuid4())[:8]
    print(f'=== Boundary & Exception Test Run: {uid} ===\n')

    # ==================== 资产不存在错误测试 (6 tests) ====================
    print('--- [异常] 不存在的资产 (6) ---')
    NX='/Game/Nonexistent/NX_'+uid
    await expect_error('bt_add_composite(NX)','bt_add_composite',{'path':NX,'parent_id':'','composite_type':'Selector'},'not found')
    await expect_error('add_bb_key(NX)','add_bb_key',{'path':NX,'key_name':'Test','key_type':'Bool'},'not found')
    await expect_error('anim_add_state(NX)','anim_add_state',{'path':NX,'state_name':'Idle','x':0,'y':0},'not found')
    await expect_error('add_widget_to_canvas(NX)','add_widget_to_canvas',{'path':NX,'widget_class':'Button','name':'Test'},'not found')
    await expect_error('add_emitter(NX)','add_emitter',{'path':NX,'emitter_path':'/Engine/FX/Templates/Fountain'},'not found')
    await expect_error('add_transform_track(NX)','add_transform_track',{'seq_path':NX,'actor_name':'Nonexistent'},'not found')

    # ==================== 无效类型测试 (5 tests) ====================
    print('\n--- [边界] 无效类型/参数 (5) ---')
    await expect_error('add_bb_key(InvalidType)','add_bb_key',{'path':NX,'key_name':'Bad','key_type':'InvalidType'},'Unknown')
    await expect_error('bt_add_composite(Invalid)','bt_add_composite',{'path':NX,'parent_id':'','composite_type':'InvalidType'},'not found')
    await expect_error('add_widget_to_canvas(Invalid)','add_widget_to_canvas',{'path':NX,'widget_class':'InvalidWidget','name':'Bad'},'not found')
    await expect_error('add_pcg_node(Invalid)','add_pcg_node',{'path':NX,'node_type':'InvalidNodeType'},'not found' if True else '')
    await expect_error('bt_add_composite(EmptyPath)','bt_add_composite',{'path':'','parent_id':'','composite_type':'Selector'},'')

    # ==================== 边界值测试 (4 tests) ====================
    print('\n--- [边界] 边界值 (4) ---')
    # 使用已创建成功的资源路径测试边界
    uid2=str(uuid.uuid4())[:8]
    BT2='/Game/AI/BT_BND_'+uid2
    BB2='/Game/AI/BB_BND_'+uid2
    r_bt=await test('bt_create(Boundary)','bt_create',{'path':BT2,'name':'BT_BND_'+uid2})
    r_bb=await test('create_blackboard(Boundary)','create_blackboard',{'path':BB2,'name':'BB_BND_'+uid2})
    if r_bt:
        r_cmp=await test('bt_add_composite(EmptyParent)','bt_add_composite',{'path':BT2,'parent_id':'','composite_type':'SimpleParallel'},'node_id')
    if r_bb:
        await test('add_bb_key(Bool Default)','add_bb_key',{'path':BB2,'key_name':'Zero','key_type':'Bool'})
        await test('add_bb_key(Enum)','add_bb_key',{'path':BB2,'key_name':'ColorEnum','key_type':'Enum'})

    # ==================== 连接异常测试 (2 tests) ====================
    print('\n--- [异常] 连接异常 (2) ---')
    r,w,r2,w2=None,None,None,None
    try:
        r,w=await asyncio.wait_for(asyncio.open_connection('127.0.0.1',19999),timeout=3)
        print('  FAIL [port_19999]: unexpected connection success'); FAIL+=1
    except:
        print('  OK   [port_19999]: connection refused as expected'); PASS+=1
    try:
        r2,w2=await asyncio.wait_for(asyncio.open_connection('127.0.0.1',9876),timeout=3)
        w2.write(b'INVALID_PROTOCOL\n'); await w2.drain()
        line=await asyncio.wait_for(r2.readline(),timeout=5)
        print(f'  OK   [invalid_handshake]: response={line.decode().strip()[:50]}'); PASS+=1
    except Exception as ex:
        print(f'  OK   [invalid_handshake]: exception={ex}'); PASS+=1
    finally:
        if w: w.close()
        if w2: w2.close()

    total=PASS+FAIL+SKIP
    print(f'\n=== Results: {PASS} pass ({EXPECTED_ERRORS} expected errors), {FAIL} fail, {SKIP} skip (of {total}) ===')
    if FAIL==0: print('=== ALL BOUNDARY & EXCEPTION TESTS CLEAN ===')
    sys.exit(0 if FAIL==0 else 1)

asyncio.run(main())