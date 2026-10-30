# Catalog stress test: 2026-09-18

> AI-generated.

## Result

All 2,660 named base-item catalog entries were granted in bounded native batches, each followed by an independently
verified removal pass. 111 completed batches added 7,185 copies and sold 2,538 surplus equipment/Ash items for exactly
1,221,770 runes (totals across test saves, not one character's balance).

| Category | Entries checked | Entries receiving 3 new copies |
|---|---:|---:|
| Weapons, shields, ammunition | 555 | 555 |
| Armor | 715 | 715 |
| Talismans | 155 | 155 |
| Goods, spells, materials, key items | 1,119 | 727 |
| Ashes of War | 116 | 116 |

- 392 goods entries got fewer than 3 copies: existing uniques and per-item carry/storage caps (inventory allocation was
  not full).
- **O Mother** produced no inventory copy, so its preservation was not exercised. Every other entry was present after
  its grant.
- Not claimed: 3 copies of every possible item, or every upgrade/affinity permutation. Catalog excluded unnamed/error
  rows; IDs validated against the active regulation.

Every completed removal pass verified: exact keeper slots, equipped-item protection, unchanged remaining inventory, exact
trader payout, inventory count delta. Goods, key items and storage unchanged. Serpent-Hunter is unsellable; extra copies
stayed. Ammunition preserved. The game auto-equipped some new ammo into empty slots during grants; nothing equipped was
replaced.

## Final-build regression

Final DLL SHA256: `E933A4BA4D52030F587372401BF9C3A1E7196ADDBE46F3E337DFEE79C57AF8B9`.

- All 155 talismans and 1,119 goods entries ran on this build. Weapon, armor and Ash batches ran on earlier builds; their
  58 successful batch records also passed a stricter retrospective slot-identity check using the original keeper order.
- Final mixed batch on the final DLL: 3 each of Bastard Sword, Serpent-Hunter, Blue Cloth Cowl, Foot Soldier Helm,
  Crimson Amber Medallion, Throwing Dagger, Spinning Slash. A real **F8** sold **12 items for exactly 2,000 runes**,
  kept all 13 equipped records, all goods and storage, and the unsellable Serpent-Hunters. Readiness/completion overlay
  seen.
- This regression is in addition to the catalog totals. All 37 automated checks passed after the final identity fix.

## Issues found and fixed

- Negative trader `sellValue`: extraction now records `unsellable`; the seller keeps those items. Missing, approximate
  or zero candidate prices still block a sale.
- Identical talismans can share a native handle: copies are now identified by inventory slot, and the exact equipped
  slot is protected. The post-removal check requires exactly one selected slot to disappear. An early test caught the
  old behaviour after one removal, before payout; the save was restored and all talismans retested successfully.
- A fast Ash grant stalled once: test stopped, save restored. Paced, bounded batches then completed all Ash IDs; cause
  unknown. Item granting is a separate, explicitly run test harness, not part of the installed mod.

## Scope

Raw captures, saves and logs stay local. Local validation only; no independent validation on other installations.
