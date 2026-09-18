"""Independently verify before/after captures, keeper counts, protected gear and trader payouts."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path


def populated(snapshot):
    return [r for r in snapshot['rows'] if r['handle'] not in (0, 0xffffffff)
            and r['item_id'] != 0xffffffff and r['quantity'] > 0]


def identity(row):
    return row['index'], row['handle'], row['item_id'], row['quantity']


def group(row):
    category, param = row['item_id'] >> 28, row['item_id'] & 0xfffffff
    if category == 0 and param != 110000 and not 50000000 <= param < 54000000:
        return category, param // 10000 * 10000
    if category in (1, 2, 8):
        return category, param
    return None


def verify(before, after, prices_path, partial=False, slot_keepers=True):
    prices = {}
    for line in Path(prices_path).read_text().splitlines():
        if not line.strip() or line.startswith('#'): continue
        category, item_id, price = line.split()
        prices[category, int(item_id)] = -1 if price == 'unsellable' else int(price)
    old, new = populated(before), populated(after)
    old_ids, new_ids = Counter(map(identity, old)), Counter(map(identity, new))
    assert not new_ids - old_ids, 'Unexpected added or altered inventory records'
    removed = list((old_ids - new_ids).elements())
    protected = {identity(before['rows'][i - 384]) for i in before['equipment_indices'] if i >= 384}
    assert not protected.intersection(removed), 'Equipped item was removed'
    assert before['equipment_indices'] == after['equipment_indices'], 'Equipped slots changed'
    old_groups, new_groups = defaultdict(list), defaultdict(list)
    for row in old:
        if group(row) is not None: old_groups[group(row)].append(row)
    for row in new:
        if group(row) is not None: new_groups[group(row)].append(row)
    expected_sales = set()
    for key, rows in old_groups.items():
        category = {0: 'weapon', 1: 'armor', 2: 'talisman', 8: 'ash_of_war'}[key[0]]
        if all(prices.get((category, r['item_id'] & 0xfffffff)) == -1 for r in rows):
            assert Counter(map(identity, rows)) == Counter(map(identity, new_groups[key])), 'Unsellable item changed'
            continue
        limit = 2 if key[0] in (0, 8) else 1
        ordered = sorted(rows, key=lambda r: (identity(r) not in protected,
                         -(r['item_id'] % 100) if key[0] == 0 else 0, r['index'] if slot_keepers else r['handle']))
        keep = max(limit, sum(identity(r) in protected for r in ordered))
        expected_sales.update(identity(r) for r in ordered[keep:])
        target = min(len(rows), keep)
        actual = len(new_groups[key])
        assert actual >= target, f'Too few copies remain for {key}: {actual} < {target}'
        if not partial: assert actual == target, f'Duplicates remain for {key}: {actual} != {target}'
    assert set(removed) <= expected_sales, 'Unexpected item selected instead of a lower-priority duplicate'
    if not partial: assert set(removed) == expected_sales, 'Sale did not match independent keeper policy'
    rune_total = 0
    for index, handle, raw_id, quantity in removed:
        category = {0: 'weapon', 1: 'armor', 2: 'talisman', 8: 'ash_of_war'}[raw_id >> 28]
        price = prices[category, raw_id & 0xfffffff]
        assert price > 0, 'Removed an item with no trader value'
        rune_total += quantity * price
    assert after['runes_candidate'] - before['runes_candidate'] == rune_total, 'Rune delta does not match trader values'
    assert before['count'] - after['count'] == len(removed), 'Unexpected inventory count delta'
    return {'verified': True, 'removed_items': len(removed), 'runes_awarded': rune_total,
            'remaining_entries': len(new), 'equipment_groups_checked': len(old_groups),
            'equipped_records_preserved': len(protected), 'partial': partial}


def verify_reload(saved, reloaded):
    # Handles and transient flags can be regenerated when the game loads.
    # Compare every occupied slot's actual item/quantity and each equipped slot.
    def stable_slots(snapshot):
        return {r['index']: (r['item_id'], r['quantity']) for r in populated(snapshot)}
    assert stable_slots(saved) == stable_slots(reloaded), 'Saved inventory did not persist exactly'
    assert saved['equipment_indices'] == reloaded['equipment_indices'], 'Saved equipment changed'
    assert saved['count'] == reloaded['count'], 'Saved inventory count changed'
    assert saved['runes_candidate'] == reloaded['runes_candidate'], 'Saved rune balance changed'
    return {'reload_verified': True, 'remaining_entries': len(populated(reloaded)),
            'runes': reloaded['runes_candidate'], 'count': reloaded['count']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before'); parser.add_argument('after'); parser.add_argument('prices')
    parser.add_argument('--partial', action='store_true')
    parser.add_argument('--legacy-handle-order', action='store_true', help='Replay pre-slot-identity weapon/armor evidence')
    parser.add_argument('--reload', action='store_true', help='Compare post-sale capture to a fresh process reload')
    args = parser.parse_args()
    before = json.loads(Path(args.before).read_text())
    after = json.loads(Path(args.after).read_text())
    result = verify_reload(before, after) if args.reload else verify(before, after, args.prices, args.partial, not args.legacy_handle_order)
    print(json.dumps(result, indent=2))
