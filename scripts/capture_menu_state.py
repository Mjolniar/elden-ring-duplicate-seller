"""Read-only menu visibility research for the pinned executable."""
import argparse, json, struct
from pathlib import Path
import discover_inventory_anchor as anchor
from stress_grant_items import GrantSession

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--pid',type=int,required=True)
p.add_argument('--base',type=int,required=True)
p.add_argument('--executable',required=True)
p.add_argument('--label',required=True)
a=p.parse_args()
anchor.PATTERN='48 8B 05 ?? ?? ?? ?? 33 DB 48 89 74 24'
discovery=anchor.discover(a.executable)
assert len(discovery['matches'])==1, discovery
m=discovery['matches'][0]
s=GrantSession(a.pid,a.base,a.executable)
try:
    manager=s.ptr(a.base+int(m['target_rva'],16))
    data=s.read(manager,0x8a0)
    frontend=s.ptr(a.base+0x3d6f8f0)
    assert s.ptr(frontend+0x18)==manager, 'Frontend/menu relationship differs'
    result={'label':a.label,'anchor':m,'anchor_bytes':s.read(a.base+int(m['instruction_rva'],16),16).hex(),
            'manager':hex(manager),'frontend':hex(frontend),'hud_state':s.read(frontend+0x78,1)[0],
            'ui_states':list(data[0x90:0xd6]),'header':data[:0x140].hex()}
    out=Path('dist/menu');out.mkdir(exist_ok=True)
    (out/(a.label+'.json')).write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!='header'}))
finally:s.close()
