"""Report catalog coverage from completed, independently verified live batches."""
import json
from collections import Counter
from pathlib import Path

root=Path('dist/stress');catalog=json.loads((root/'catalog.json').read_text())
prefixes=('ashes_slow_','weapons_','armor_','talismans_','goods_')
covered={};batches=[];limited=[]
for folder in sorted(root.iterdir()):
 if not folder.is_dir() or not folder.name.startswith(prefixes) or not (folder/'verification.json').exists():continue
 result=json.loads((folder/'verification.json').read_text())
 assert result['verified'] and not result['partial']
 grants=json.loads((folder/'grants.json').read_text())
 for item in grants:
  covered[item['id']]=item
  if item['added']!=3:limited.append(item)
 batches.append({'name':folder.name,**result})
categories={}
for category in sorted({x['category'] for x in catalog}):
 total=[x for x in catalog if x['category']==category]
 done=[x for x in covered.values() if x['category']==category]
 categories[category]={'catalog_ids':len(total),'verified_ids':len(done),'three_added':sum(x['added']==3 for x in done)}
report={'categories':categories,'verified_batches':len(batches),'copies_added':sum(x['added'] for x in covered.values()),
 'sold_items':sum(x['removed_items'] for x in batches),'runes_awarded':sum(x['runes_awarded'] for x in batches),
 'limited_items':limited,'missing_ids':[x for x in catalog if x['id'] not in covered],'batches':batches}
(root/'coverage.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:v for k,v in report.items() if k not in ('limited_items','missing_ids','batches')},indent=2))
