"""Bounded explicit grant/sell/independent verification batches for live testing."""
import argparse,json,subprocess,sys,time
from pathlib import Path
from stress_grant_items import GrantSession
from verify_native_sale import verify

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--pid',type=int,required=True);p.add_argument('--base',type=lambda x:int(x,0),required=True)
p.add_argument('--executable',required=True);p.add_argument('--category',type=int,required=True)
p.add_argument('--name',required=True);p.add_argument('--start',type=int,default=0);p.add_argument('--limit',type=int,default=9999)
a=p.parse_args()
catalog=json.loads(Path('dist/stress/catalog.json').read_text())
count=len([x for x in catalog if x['category']==a.category])
for start in range(a.start,min(count,a.start+a.limit),25):
 name=f'{a.name}_{start:04d}';out=Path('dist/stress')/name
 subprocess.run([sys.executable,'scripts/stress_grant_items.py','--pid',str(a.pid),'--base',str(a.base),'--executable',a.executable,'--category',str(a.category),'--start',str(start),'--limit',str(min(25,a.start+a.limit-start)),'--batch',name,'--execute'],check=True)
 before=json.loads((out/'after_grant.json').read_text())
 log=Path('dist/probe/ERDuplicateSeller.log');offset=log.stat().st_size
 Path('dist/probe/runtime_command.txt').write_text(f'sell_duplicates # stress {name} {time.time_ns()}')
 deadline=time.monotonic()+60
 while time.monotonic()<deadline:
  time.sleep(.5)
  with log.open('rb') as f:f.seek(offset);tail=f.read().decode(errors='replace')
  if 'Sold ' in tail:break
  if 'blocked' in tail.lower() or 'failed' in tail.lower() or 'fault' in tail.lower() or 'autosell stopped:' in tail.lower():raise RuntimeError(tail)
 else:raise RuntimeError('Sale did not complete within 60 seconds: '+tail)
 session=GrantSession(a.pid,a.base,a.executable)
 try:after=session.snapshot()
 finally:session.close()
 (out/'after_sale.json').write_text(json.dumps(after,indent=2))
 assert before['key_rows']==after['key_rows'],'Key inventory changed during sale'
 for label in ('storage_rows','storage_key_rows'):
  assert before[label]==after[label],'Storage changed during sale'
 result=verify(before,after,'dist/probe/sell_values.txt')
 (out/'verification.json').write_text(json.dumps(result,indent=2))
 print(name,json.dumps(result),flush=True)
 time.sleep(1)
