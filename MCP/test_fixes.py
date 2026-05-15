import socket, json, uuid

H,P='127.0.0.1',9876
def s(m,p=None):
    sk=socket.socket();sk.settimeout(15);sk.connect((H,P))
    rq=json.dumps({'id':str(uuid.uuid4()),'method':m,'params':p or {}})+'\n'
    sk.sendall(rq.encode())
    d=b'';import time;dl=time.time()+15
    while time.time()<dl:
        try:c=sk.recv(4096);d+=c
        except:break
        if b'\n' in d:break
    sk.close();return json.loads(d.decode().strip()).get('result',{}) if d else {}

def chk(label, ok, extra=''):
    print(f'{"✅" if ok else "❌"} {label}{" — " + extra if extra else ""}')
    return ok

p=t=0

# Fix 1: create_widget path fix
r=s('create_widget',{'path':'/Game/Test/','name':'WBP_FixTest'})
wp=r.get('path','')
t+=1
p+=chk('Fix1: create_widget returns full path', '/WBP_FixTest' in str(wp) and not str(wp).endswith('/'), wp)

if wp:
    r2=s('add_widget_to_canvas',{'path':wp,'widget_class':'TextBlock','name':'TitleText'})
    t+=1
    p+=chk('Fix1: add_widget_to_canvas with full path', 'status' in r2, r2.get('status', r2.get('error','?')))

    r3=s('add_widget_to_canvas',{'path':wp,'widget_class':'Button','name':'BtnOK'})
    t+=1
    p+=chk('Fix1: add button to canvas', 'status' in r3, r3.get('status', r3.get('error','?')))

# Fix 2: AnimInstance parent class
r=s('bp_create',{'path':'/Game/Test/ABP_FixTest','parent_class':'AnimInstance'})
t+=1
p+=chk('Fix2: bp_create with AnimInstance', 'status' in r, r.get('status', r.get('error','?')))

ap=r.get('path','')
if ap:
    r2=s('anim_add_state',{'path':ap,'state_name':'Idle','x':0,'y':0})
    t+=1
    p+=chk('Fix2: anim_add_state on created AnimBP', 'status' in r2, r2.get('status', r2.get('error','?')))

    r3=s('anim_add_state',{'path':ap,'state_name':'Run','x':300,'y':0})
    t+=1
    p+=chk('Fix2: add second state', 'status' in r3, r3.get('status', r3.get('error','?')))

    r4=s('anim_add_transition',{'path':ap,'from_state':'Idle','to_state':'Run','condition':''})
    t+=1
    p+=chk('Fix2: anim_add_transition', 'status' in r4, r4.get('status', r4.get('error','?')))

# Fix 3: add_actor_to_sequencer
r=s('create_sequence',{'path':'/Game/Test/','name':'LS_FixTest'})
sp=r.get('path','')
t+=1
p+=chk('Fix3: create_sequence', 'status' in r, r.get('status', r.get('error','?')))

if sp:
    r2=s('add_actor_to_sequencer',{'seq_path':sp,'actor_name':'StaticMeshActor_0'})
    t+=1
    p+=chk('Fix3: add_actor_to_sequencer (new cmd!)', 'status' in r2 or 'binding_guid' in r2, r2.get('status', r2.get('error','?')))

    r3=s('set_keyframe',{'seq_path':sp,'actor_name':'StaticMeshActor_0','time':2.0,'x':500,'y':0,'z':300})
    t+=1
    p+=chk('Fix3: set_keyframe after add_actor', 'status' in r3, r3.get('status', r3.get('error','?')))

print(f'\n{"="*40}')
print(f'  {p}/{t} 通过' if p==t else f'  ⚠ {p}/{t} 通过')
print(f'{"="*40}')