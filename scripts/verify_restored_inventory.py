"""Verify a restored save's carried inventory, storage, equipment and runes."""
import argparse
import json
from pathlib import Path
from verify_native_sale import verify_reload

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('baseline')
p.add_argument('restored')
p.add_argument('--output', required=True)
a = p.parse_args()
before = json.loads(Path(a.baseline).read_text())
after = json.loads(Path(a.restored).read_text())
result = verify_reload(before, after)
for label in ('key_rows', 'storage_rows', 'storage_key_rows'):
    def stable(snapshot):
        return {r['index']: (r['item_id'], r['quantity']) for r in snapshot[label]
                if r['handle'] not in (0, 0xffffffff) and r['quantity'] > 0
                and r['item_id'] != 0xffffffff}
    assert stable(before) == stable(after), f'{label} did not restore exactly'
result['key_inventory_and_storage_restored'] = True
Path(a.output).write_text(json.dumps(result, indent=2))
print(json.dumps(result))
