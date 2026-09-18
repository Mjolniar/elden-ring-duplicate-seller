# Provisional native seller for 2.7.1.0

## Operation

The opt-in native seller runs from F8 (hotkey_vk) or the explicit sell_duplicates diagnostic command. The game must be in the foreground for the hotkey. A top-right, click-through indicator displays readiness, progress, stopped runs and the final payout. Use it while idle and do not change equipment, travel or pick up items during a run.

It keeps at least two copies of each normalized weapon/shield family and each Ash of War ID, and one of each armor/talisman ID. Every consumable is preserved. Equipped items take priority, then higher weapon upgrade levels. If more than the keep count are equipped, all equipped copies remain. Spells, consumables, materials, key items, ammunition and unarmed sentinel entries are excluded from this provisional native path. Only singleton equipment rows with exact positive sellValue entries are accepted; an unknown or approximate candidate price blocks the run before any removal.

The reference config is dry-run by default. Native use requires native_sale_enabled=true, supported_game_version=2.7.1.0 and dry_run=false. native_sale_max_items=1 limits testing to one sale; zero permits the full plan. Config is reloaded on each trigger. Metadata and price tables are loaded at process start.

## Validated executable interfaces

Executable SHA256: 1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891. Image size: 98621952. All addresses below are module-relative RVAs.

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
| Equipped slot indices | PlayerGameData+0x2b8, 22 signed indices, -1 means empty |
| Rune balance | PlayerGameData+0x6c |

The implementation validates the executable version, image size and exact function-anchor bytes on every request. Before each removal it rereads inventory, recalculates eligibility and checks equipped indices. After removal it compares all item identities/quantities and the count, then awards and checks the exact trader value. Exceptions or discrepancies after mutation latch a fault until restart. Successful runs request the game's own save operation. It does not serialize or edit the save file itself.

Each run writes native_sale_<uptime>.txt beside the DLL, including the plan, each intended operation, confirmed removal/payout and save request. The diagnostic scripts capture externally and independently check final keep counts, keeper choice, equipment preservation, unaffected inventory and exact rune delta.

This is a provisional worker-thread adapter. Native calls were identified from source references and current executable disassembly; this is not a general compatibility guarantee for future game updates, multiplayer sessions, or concurrent inventory changes.

## Copy identity

The native reader assigns one-based normal-slot identities to the policy. Talisman copies can share their native handle, so a handle alone cannot identify a copy. Equipped protection uses the exact equipped slot. After removal, a map from slot index to handle/item/quantity must equal the original map with exactly the selected slot removed. The legacy diagnostic reader retains its handle identities unless the native-only layout option is enabled.

## Unsellable metadata

The price extractor emits `unsellable` for negative regulation sell values. The parser preserves this explicit marker as -1 and the policy excludes those items before constructing the sale plan. Missing prices still block a selected candidate; they are not treated as unsellable. All existing nonnegative prices are unchanged. A live three-copy Serpent-Hunter test preserved all three and allowed the surrounding weapon batch to complete.

## Source references

Interface research used The Grand Archives at commit 7926205c5a2ed236dd31278c4f5579c964ceec35:

- [RemoveItem and AddSoul interface references](https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA/tree/7926205c5a2ed236dd31278c4f5579c964ceec35/CheatTable/CheatEntries/Open%20-%20The%20Grand%20Archives%20-%20Elden%20Ring/MiscWIP/Dependencies/Global%20Functions).
- [Save Request reference](https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA/blob/7926205c5a2ed236dd31278c4f5579c964ceec35/CheatTable/CheatEntries/Open%20-%20The%20Grand%20Archives%20-%20Elden%20Ring/Statistics/SaveLoad2/Save%20Request.cea). Current executable instructions at 0x67b349–0x67b369 independently corroborate the singleton, active-slot check and save-request field.
- [Inventory allocation layout reference](https://docs.rs/eldenring/latest/src/eldenring/cs/player_game_data.rs.html).

The C++ adapter and indicator are new local implementations; the downloaded reference repository remains in ignored dist/research and is not part of the shipped package.
