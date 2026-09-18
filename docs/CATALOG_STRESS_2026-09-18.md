# Catalog stress test — September 18, 2026

## Result

All 2,660 entries in the named base-item catalog were attempted in bounded native grant batches and followed by independently verified removal passes. The 111 completed catalog batches added 7,185 copies and sold 2,538 surplus equipment/Ash items for exactly 1,221,770 runes. These totals span test saves and are not the final character balance.

| Category | Catalog entries checked | Entries receiving three new copies |
|---|---:|---:|
| Weapons, shields and ammunition | 555 | 555 |
| Armor | 715 | 715 |
| Talismans | 155 | 155 |
| Goods, spells, materials and key items | 1,119 | 727 |
| Ashes of War | 116 | 116 |

The game limited 392 goods entries to fewer than three additions. Existing unique items and per-item carry/storage caps account for these limits; the inventory allocation itself was not full. The catalog entry named **O Mother** produced no inventory copy, so preservation of that entry was not exercised. Every other catalog entry was present after its grant. The test does not claim three copies of every possible item, nor coverage of every upgrade/affinity permutation. Catalog construction excluded unnamed/error rows and validated IDs against the active regulation.

All completed removal passes independently verified the exact intended keeper slots, equipped-item protection, unchanged remaining inventory, exact trader payout and inventory count delta. Goods, key inventory and storage remained unchanged during removal. Serpent-Hunter is explicitly unsellable and its extra copies remain. Ammunition is preserved. The game automatically equipped some new ammunition into empty ammunition slots during grants; no previously equipped item was replaced.

## Final-build regression

Final DLL SHA256: `E933A4BA4D52030F587372401BF9C3A1E7196ADDBE46F3E337DFEE79C57AF8B9`.

All 155 talismans and all 1,119 goods entries were exercised on this build. Earlier weapon, armor and Ash batches ran on preceding builds; their 58 successful batch records also passed a stricter retrospective slot-identity comparison using the original keeper ordering. A final mixed-category batch on the final DLL added three each of Bastard Sword, Serpent-Hunter, Blue Cloth Cowl, Foot Soldier Helm, Crimson Amber Medallion, Throwing Dagger and Spinning Slash. A real **F8** press sold **12 items for exactly 2,000 runes**, preserved all 13 equipped records, all goods and storage, and kept unsellable Serpent-Hunters. The readiness/completion overlay was observed during testing.

The final mixed regression is additional to the catalog totals above. All 37 automated checks passed after the final native identity fix.

## Issues found and corrected

- Some trader `sellValue` values are negative. Price extraction now records these explicitly as `unsellable`, and the seller preserves them. Missing, approximate or zero candidate prices still block a real sale.
- Identical talismans can share a native handle. The seller now identifies copies by inventory slot and protects the exact equipped slot. Its post-removal check requires precisely one selected slot to disappear, preserving every other slot. An early test caught the old behavior after one removal and before payout; the original save was restored before retesting all talismans successfully.
- An initial rapid Ash grant stalled. That test was stopped and the original save restored. Paced, bounded batches completed all Ash IDs; the cause of the initial stall was not established. Item granting is a separate, explicitly invoked test harness and is not part of the installed mod.

## Evidence scope

Raw captures, saves and logs remain local and are not included. This report describes local validation, not independent validation on other installations.
