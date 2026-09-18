"""Capture and independently verify an already-completed manual stress-test sale."""
import argparse,json
from pathlib import Path
from stress_grant_items import GrantSession
from verify_native_sale import verify
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--pid',type=int,required=True);p.add_argument('--base',type=lambda x:int(x,0),required=True)
p.add_argument('--executable',required=True);p.add_argument('--batch',required=True)
a=p.parse_args();out=Path('dist/stress')/a.batch
before=json.loads((out/'after_grant.json').read_text());session=GrantSession(a.pid,a.base,a.executable)
try:after=session.snapshot()
finally:session.close()
(out/'after_sale.json').write_text(json.dumps(after,indent=2))
for field in ('key_rows','storage_rows','storage_key_rows'):assert before[field]==after[field],f'{field} changed during removal'
result=verify(before,after,'dist/probe/sell_values.txt')
(out/'verification.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
