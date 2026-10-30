# Provisional native seller for 2.7.1.0

> AI-generated.

## Operation

Opt-in. Triggered by F8 (`hotkey_vk`, game in foreground) or the `sell_duplicates` diagnostic command. A top-right
click-through indicator shows readiness, progress, stopped runs and payout. Use while idle; no equipment changes,
travel or pickups during a run.

Keeps at least 2 per normalized weapon/shield family and per Ash of War ID, 1 per armor/talisman ID. All consumables
kept. Priority: equipped, then higher weapon upgrade. If more than the keep count are equipped, all equipped stay.
Excluded: spells, consumables, materials, key items, ammunition, unarmed sentinel entries. Only singleton equipment rows
with exact positive `sellValue` are accepted; an unknown or approximate candidate price blocks the run before any
removal.

Reference config is dry-run. Native use needs `native_sale_enabled=true`, `supported_game_version=2.7.1.0`,
`dry_run=false`. `native_sale_max_items=1` limits a test to one sale; 0 = full plan. Config reloads per trigger;
metadata and price tables load at process start.

## Validated executable interfaces

Executable SHA256 `1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891`, image size 98621952. Module-relative
RVAs:

| Interface | RVA / layout |
|---|---|
| GameDataMan singleton | 0x3d61f98; getter at 0x256560 |
| PlayerGameData | [GameDataMan]+8 |
| RemoveItem | 0x24d1e0; (inventory controller, combined index, release underlying item=1) |
| AddSoul | 0x25e0e0; (PlayerGameData, amount); returns actual delta |
| GameMan singleton | 0x3d6d988; getter at 0x67aba0 |
| Native save request | GameMan+0xb72 = 1; active save slot at +0xac0 |
| Inventory controller | [PlayerGameData+0x5d0], validated equal to PlayerGameData+0x408 |
| Normal allocation | PlayerGameData+0x414 capacity, +0x418 array, +0x420 count |
| Normal-array accessor | controller+0x40, checked equal to normal array |
| Key-item capacity / index bias | controller+0x1c = 384; native index = normal slot+384 |
| Equipped slot indices | PlayerGameData+0x2b8, 22 signed indices, -1 = empty |
| Rune balance | PlayerGameData+0x6c |

Every request validates version, image size and exact function-anchor bytes. Before each removal: reread inventory,
recompute eligibility, check equipped indices. After: compare all item identities/quantities and count, award and check
the exact trader value. Exceptions or discrepancies after mutation latch a fault until restart. Successful runs request
the game's own save; the save file is never edited directly.

Each run writes `native_sale_<uptime>.txt` beside the DLL: plan, each intended operation, confirmed removal/payout, save
request. Diagnostic scripts independently check keep counts, keeper choice, equipment, unaffected inventory and exact
rune delta.

Provisional worker-thread adapter. Native calls identified from source references and current disassembly. No guarantee
for future game updates, multiplayer, or concurrent inventory changes.

## Copy identity

The native reader gives the policy one-based normal-slot identities. Talisman copies can share a native handle, so a
handle cannot identify a copy. Equipped protection uses the exact equipped slot. After removal, the slot-to-
handle/item/quantity map must equal the original minus exactly the selected slot. The legacy diagnostic reader keeps
handle identities unless the native-only layout option is on.

## Unsellable items

The extractor emits `unsellable` for negative regulation sell values; the parser stores -1 and the policy excludes those
items before planning. Missing prices still block a selected candidate (not treated as unsellable). Nonnegative prices
unchanged. A live three-copy Serpent-Hunter test kept all three and the surrounding weapon batch completed.

## Source references

The Grand Archives, commit 7926205c5a2ed236dd31278c4f5579c964ceec35:

- [RemoveItem and AddSoul](https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA/tree/7926205c5a2ed236dd31278c4f5579c964ceec35/CheatTable/CheatEntries/Open%20-%20The%20Grand%20Archives%20-%20Elden%20Ring/MiscWIP/Dependencies/Global%20Functions).
- [Save Request](https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA/blob/7926205c5a2ed236dd31278c4f5579c964ceec35/CheatTable/CheatEntries/Open%20-%20The%20Grand%20Archives%20-%20Elden%20Ring/Statistics/SaveLoad2/Save%20Request.cea). Executable instructions at 0x67b349-0x67b369 independently confirm the singleton, active-slot check and save-request field.
- [Inventory allocation layout](https://docs.rs/eldenring/latest/src/eldenring/cs/player_game_data.rs.html).

The C++ adapter and indicator are new implementations. The downloaded reference repository stays in ignored
`dist/research` and is not shipped.
