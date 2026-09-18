# Changelog

## 0.1.0-beta.2

- Double-click installer with configuration picker, package checksums, backup and rollback.
- Adds its DLL once while preserving other mods. Updates preserve settings and prices.
- Uninstall removes registration and moves mod files into backup.
- Prices resolve beside the DLL; relative and absolute overrides are supported.
- Choose preview or real selling during installation; Settings.cmd switches modes later.
- Added installer integration tests and relative/default/missing price-file checks.
- Menu appearance remains pending; sale/retention logic is unchanged.

## 0.1.0-beta.1

- Initial distribution candidate for executable 2.7.1.0 / in-game 1.17.1.
- F8 preview/sale; keeps 2 weapons/shields and Ashes, 1 armor/talisman, and all goods/ammunition.
- Exact trader prices; unsellable items preserved.
- Protects equipped slot identities, including identical talismans that share handles.
- Checks each removal and payout, then requests a normal game save.
- Compact menu indicator with a 1.5-second result timeout. Positive menu visibility/timing validation is pending.
- Clean distribution configuration, numeric base-game price table, optional path helper, installation instructions and checksums.
