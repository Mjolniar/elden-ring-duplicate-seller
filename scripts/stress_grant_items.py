"""Explicit test-only native item grants for the pinned 2.7.1.0 executable.

Uses game-owned ItemGive. Writes only newly allocated test input/code/output buffers.
Requires --execute and a named batch; no background watcher or mod integration.
"""
import argparse,ctypes as c,hashlib,json,struct,time,functools
from ctypes import wintypes as w
from pathlib import Path
from collections import Counter
import capture_candidate_inventory as reader

PIN='1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891'
GIVE_RVA=0x561400
GIVE_BYTES=bytes.fromhex('40 55 56 57 41 54 41 55 41 56 41 57 48 8d ac 24 70 ff ff ff 48 81 ec 90 01 00 00 48 c7 45 c8 fe')
ANCHOR_RVA=0x5ac7ee
ANCHOR_BYTES=bytes.fromhex('48 8b 0d cb f2 7b 03 c7 44 24 50 ff ff ff ff')

class GrantSession:
 def __init__(self,pid,base,executable):
  assert hashlib.sha256(Path(executable).read_bytes()).hexdigest()==PIN,'Unsupported executable'
  self.pid,self.base,self.executable=pid,base,executable
  k=self.k=c.WinDLL('kernel32',use_last_error=True)
  k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
  k.CloseHandle.argtypes=[w.HANDLE]
  k.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
  k.WriteProcessMemory.argtypes=k.ReadProcessMemory.argtypes
  k.VirtualAllocEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD,w.DWORD];k.VirtualAllocEx.restype=c.c_void_p
  k.VirtualFreeEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD]
  k.VirtualProtectEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD,c.POINTER(w.DWORD)]
  k.FlushInstructionCache.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t]
  k.CreateRemoteThread.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,c.c_void_p,c.c_void_p,w.DWORD,c.c_void_p];k.CreateRemoteThread.restype=w.HANDLE
  k.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD]
  k.GetExitCodeThread.argtypes=[w.HANDLE,c.POINTER(w.DWORD)]
  k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]
  self.h=k.OpenProcess(0x1000|0x10|0x20|0x08|0x02,False,pid)
  if not self.h:raise c.WinError(c.get_last_error())
  name,length=c.create_unicode_buffer(32768),w.DWORD(32768)
  assert k.QueryFullProcessImageNameW(self.h,0,name,c.byref(length))
  assert Path(name.value).resolve()==Path(executable).resolve()
  assert self.read(base+GIVE_RVA,len(GIVE_BYTES))==GIVE_BYTES,'ItemGive code differs'
  assert self.read(base+ANCHOR_RVA,len(ANCHOR_BYTES))==ANCHOR_BYTES,'MapItemMan anchor differs'
  self.manager=self.ptr(base+0x3d6bac0);assert self.manager>0x10000
  self.player=self.ptr(self.ptr(base+0x3d61f98)+8);assert self.player>0x10000
  reader.discover=functools.lru_cache(maxsize=1)(reader.discover)
 def read(self,address,size):
  buf,n=c.create_string_buffer(size),c.c_size_t()
  assert self.k.ReadProcessMemory(self.h,address,buf,size,c.byref(n)) and n.value==size
  return buf.raw
 def ptr(self,address):return struct.unpack('<Q',self.read(address,8))[0]
 def snapshot(self):
  s=reader.capture(self.pid,self.base,self.executable)
  # Key-item allocation lives beside the normal allocation in the same controller.
  controller=int(s['controller'],16);array=self.ptr(controller+0x20)
  capacity=struct.unpack('<I',self.read(controller+0x1c,4))[0];assert capacity==384
  block=self.read(array,capacity*24)
  s['key_rows']=[dict(zip(('handle','item_id','quantity','display_index','unknown_10','unknown_14'),struct.unpack_from('<6I',block,i*24)),index=i) for i in range(capacity)]
  chest=self.ptr(self.player+0x8d0)
  for label,offset,expected in [('storage_rows',0,1920),('storage_key_rows',16,128)]:
   capacity=struct.unpack('<I',self.read(chest+0x0c+offset,4))[0];assert capacity==expected
   array=self.ptr(chest+0x10+offset);block=self.read(array,capacity*24)
   assert block==self.read(array,capacity*24),'Storage changed during capture'
   s[label]=[dict(zip(('handle','item_id','quantity','display_index','unknown_10','unknown_14'),struct.unpack_from('<6I',block,i*24)),index=i) for i in range(capacity)]
  return s
 def grant(self,records,journal=None):
  assert 0<len(records)<=10
  assert self.ptr(self.base+0x3d6bac0)==self.manager
  assert self.ptr(self.ptr(self.base+0x3d61f98)+8)==self.player
  input_data=struct.pack('<I',len(records))+b''.join(struct.pack('<4I',id,qty,0xffffffff,0xffffffff) for id,qty in records)
  k=self.k; mem=k.VirtualAllocEx(self.h,None,0x2000,0x3000,0x04);assert mem
  code_address=mem+0x1000;output=mem+0x400
  # Windows x64 call ABI: reserve aligned shadow space; four arguments.
  code=b'\x48\x83\xec\x28'+b'\x48\xb9'+struct.pack('<Q',self.manager)+b'\x48\xba'+struct.pack('<Q',mem)+b'\x49\xb8'+struct.pack('<Q',output)+b'\x45\x31\xc9'+b'\x48\xb8'+struct.pack('<Q',self.base+GIVE_RVA)+b'\xff\xd0\x31\xc0\x48\x83\xc4\x28\xc3'
  def write(addr,data):
   n=c.c_size_t();buf=c.create_string_buffer(data)
   assert k.WriteProcessMemory(self.h,addr,buf,len(data),c.byref(n)) and n.value==len(data)
  write(mem,input_data);write(code_address,code)
  old=w.DWORD();assert k.VirtualProtectEx(self.h,code_address,0x1000,0x20,c.byref(old))
  assert k.FlushInstructionCache(self.h,code_address,len(code))
  thread_id=w.DWORD()
  thread=k.CreateRemoteThread(self.h,None,0,code_address,None,0,c.byref(thread_id));assert thread
  if journal:journal.write_text(json.dumps({'pid':self.pid,'thread_id':thread_id.value,'allocation':hex(mem),'records':records,'state':'pending'},indent=2))
  wait=k.WaitForSingleObject(thread,10000)
  if wait!=0:raise RuntimeError('Native grant did not finish; buffers deliberately retained. Do not retry blindly.')
  exitcode=w.DWORD();assert k.GetExitCodeThread(thread,c.byref(exitcode));k.CloseHandle(thread)
  k.VirtualFreeEx(self.h,mem,0,0x8000)
  assert exitcode.value==0,f'Native grant exception: {exitcode.value:x}'
  if journal:journal.write_text(json.dumps({'pid':self.pid,'thread_id':thread_id.value,'records':records,'state':'completed'},indent=2))
 def close(self):self.k.CloseHandle(self.h)

def quantities(s):
 result=Counter()
 for r in s['rows']+s.get('key_rows',[])+s.get('storage_rows',[])+s.get('storage_key_rows',[]):
  if r['handle'] not in(0,0xffffffff) and r['quantity']>0 and r['item_id']!=0xffffffff:result[r['item_id']]+=r['quantity']
 return result

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--pid',type=int,required=True);p.add_argument('--base',type=lambda x:int(x,0),required=True);p.add_argument('--executable',required=True)
 p.add_argument('--catalog',default='dist/stress/catalog.json');p.add_argument('--batch',required=True);p.add_argument('--ids');p.add_argument('--category',type=int)
 p.add_argument('--start',type=int,default=0);p.add_argument('--limit',type=int,default=100);p.add_argument('--execute',action='store_true');p.add_argument('--snapshot-only',action='store_true')
 a=p.parse_args();catalog=json.loads(Path(a.catalog).read_text());ids={int(x,0) for x in a.ids.split(',')} if a.ids else None
 selected=[x for x in catalog if (ids is None or x['id'] in ids) and (a.category is None or x['category']==a.category)][a.start:a.start+a.limit]
 if not a.snapshot_only:assert a.execute,'Use --execute to authorize this explicit batch'
 out=Path('dist/stress')/a.batch;out.mkdir(exist_ok=False)
 session=GrantSession(a.pid,a.base,a.executable)
 try:
  before=session.snapshot();(out/'before.json').write_text(json.dumps(before,indent=2))
  if a.snapshot_only:print('Snapshot saved:',out);raise SystemExit(0)
  assert before['count']+sum(3 if x['category'] in(0,1,2,8) and not 50000000<=x['id']<54000000 else 1 for x in selected)<before['capacity']-32,'Batch would exceed normal allocation; use a smaller batch'
  print('Granting',len(selected),'item IDs, three copies each',flush=True)
  progress=[]
  side_effects=[]
  tutorials_path=Path('dist/stress/automatic_tutorials.json')
  tutorials={x['id']:x['name'] for x in json.loads(tutorials_path.read_text())} if tutorials_path.exists() else {}
  group_size=10 if all(x['category']==4 for x in selected) else 3
  for idx in range(0,len(selected),group_size):
   group=selected[idx:idx+group_size]
   pre=session.snapshot();old=quantities(pre)
   # Native ItemGive supports up to ten entries. Three equipment types use nine.
   records=[]
   for item in group:
    individual=item['category'] in(0,1,2,8) and not 50000000<=item['id']<54000000
    records.extend([(item['id'],1)]*3 if individual else [(item['id'],3)])
   session.grant(records,out/'pending_grant.json')
   time.sleep(1);post=session.snapshot();new=quantities(post)
   removed=old-new;added=new-old
   assert not removed,f'Grant removed unrelated inventory: {removed}'
   extra_ids=set(added)-{item['id'] for item in group}
   assert extra_ids<=tutorials.keys(),f'Grant added unrelated item: {added}'
   for id in extra_ids:side_effects.append({'id':id,'name':tutorials[id],'added':added[id],'reason':'game tutorial awarded during grant'})
   (out/'grant_side_effects.json').write_text(json.dumps(side_effects,indent=2))
   assert pre['runes_candidate']==post['runes_candidate'],'Rune balance changed during grant'
   for slot,(prior,current) in enumerate(zip(pre['equipment_indices'],post['equipment_indices'])):
    if prior==current:continue
    # The game auto-equips newly acquired ammunition into empty ammo slots.
    assert prior==-1 and slot in (6,7,8,9) and 384<=current<384+post['capacity'],'Existing equipped slots changed during grant'
    equipped_id=post['rows'][current-384]['item_id']
    assert 50000000<=equipped_id<54000000 and equipped_id in {x['id'] for x in group},'Unexpected auto-equip during grant'
   for item in group:
    assert added[item['id']]<=3,'Native grant exceeded requested quantity'
    progress.append({**item,'requested_add':3,'before':old[item['id']],'added':added[item['id']],'after':new[item['id']]})
   (out/'grants.json').write_text(json.dumps(progress,indent=2))
   if idx==0:print(len(progress),'/',len(selected),'item IDs verified',flush=True)
  final=session.snapshot();(out/'after_grant.json').write_text(json.dumps(final,indent=2))
  print(json.dumps({'ids':len(selected),'copies_added':sum(x['added'] for x in progress),'limited_ids':sum(x['added']!=3 for x in progress),'normal_count':final['count'],'runes':final['runes_candidate']}),flush=True)
 finally:session.close()
