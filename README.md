# Elden Ring Duplicate Seller

> [!WARNING]
> **AI-generated.** Code and docs written by AI coding agents. No independent review. Beta. Offline only. Back up
> your save. Use at your own risk.

Version 0.1.0-beta.2. Press **F8** to sell surplus carried equipment at trader rune value.

| Item | Copies kept |
|---|---:|
| Weapon/shield, per family | 2 |
| Ash of War | 2 |
| Armor piece | 1 |
| Talisman, per ID | 1 |
| Consumables, spells, materials, keys, ammo | All |

- Equipped copies are always kept, even above the limit. Higher upgrade levels are kept next.
- Affinities share one family. Talisman tiers and altered armor count separately.
- Unsellable items and storage are not touched.

## Requirements

- Windows x64. Elden Ring executable 2.7.1.0 (game 1.17.1). Other versions are refused.
- Mod Engine 2. Tested with Mod Engine 2 2.1.0, Seamless Co-op 2.0.1, Item and Enemy Randomizer 0.12.2, no other
  players connected.
- Microsoft Visual C++ 2015-2022 x64 runtime.
- Not for official online (Easy Anti-Cheat) or multiplayer sessions.

## Install

1. Extract the ZIP. Run `ERDuplicateSeller/Install.cmd`.
2. Select your Mod Engine 2 `config_eldenring.toml`. Choose **selling** or **preview**.
3. Launch offline modded. Press F8.

- `Settings.cmd`: switch mode.
- `Uninstall.cmd`: remove registration, move mod files to a backup.
- Update: run `Install.cmd` from the new download. Settings and prices are kept.

Prices come from the base-game regulation. Modded regulation: re-extract prices. Details: [INSTALL.md](INSTALL.md).

## Status

- Stress test: 111 catalog batches, 2,660 entries, 2,538 copies sold for 1,221,770 runes.
  [Report](docs/CATALOG_STRESS_2026-09-18.md).
- Menu indicator: hidden in gameplay and F8 verified. Pause menu/Inventory visibility and timeout not verified.
  [Details](docs/MENU_INDICATOR.md).
- Selling runs on a worker thread. During a run: stand still; do not change equipment, pick up items or travel.
  [Adapter](docs/NATIVE_2710.md).

## Build

Visual Studio 2022 C++ tools, Windows SDK, CMake 3.21+. Python and PowerShell enable extra checks.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Output: `build/Release/ERDuplicateSeller.dll`. `scripts/` holds developer diagnostics and item-grant tools. Not
installed. Do not run them on a save you value.

## License

None selected. No permissive license implied. Credits: [CREDITS.md](CREDITS.md). No game executables, regulation
files, third-party binaries, saves or cheat tables included.
