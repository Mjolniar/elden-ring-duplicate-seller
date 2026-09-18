# Elden Ring Duplicate Seller

**0.1.0-beta.1 — offline, experimental.** Press **F8** to sell surplus carried equipment for its trader rune value.

| Item | Copies kept |
|---|---:|
| Each weapon/shield family | 2 |
| Each Ash of War | 2 |
| Each armor piece | 1 |
| Each talisman ID | 1 |
| Consumables, spells, materials, keys and ammunition | All |

Equipped copies are always protected, even above the keep count. Higher weapon upgrade levels are preferred next. Weapon affinities share a base family; different talisman tiers and altered armor IDs remain separate. Unsellable equipment stays in inventory. Storage is not processed.

## Compatibility

- Windows x64; Elden Ring executable **2.7.1.0** (in-game **1.17.1**). Other versions are refused.
- Mod Engine 2 DLL loading. Tested locally with Mod Engine 2 2.1.0, Seamless Co-op 2.0.1 and Item and Enemy Randomizer 0.12.2, without other players connected.
- Requires the Microsoft Visual C++ 2015–2022 x64 runtime.
- Official online/Easy Anti-Cheat and multiplayer sessions are unsupported. Back up your save and use an offline modded launch.

## Installation

See [INSTALL.md](INSTALL.md). The download starts in **preview mode** (`dry_run=true`). Enable actual selling after reviewing the plan. The bundled price table was extracted from the installed base-game regulation; modified prices require re-extraction from the regulation your game actually loads.

## Status

The seller completed 111 verified catalog batches across 2,660 attempted catalog entries. The test sold 2,538 surplus equipment/Ash copies for exactly 1,221,770 runes. Individual item caps prevented adding three of some goods. See [test coverage](docs/CATALOG_STRESS_2026-09-18.md).

The new compact indicator is designed to show only in menus and restore the F8 hint 1.5 seconds after completion. Hidden gameplay behavior and F8 were verified. **Positive pause-menu/Inventory visibility and live timeout appearance are still pending verification.** This beta must not be described as a finished stable release.

Native routines run on a worker thread. Stand still while selling; do not change equipment, collect items or travel during a run. See [adapter details](docs/NATIVE_2710.md) and [indicator validation](docs/MENU_INDICATOR.md).

## Build and test

Requires Visual Studio 2022 C++ tools, Windows SDK and CMake 3.21+. Python and PowerShell enable additional script checks.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

DLL output: `build/Release/ERDuplicateSeller.dll`. The `scripts/` folder also contains developer diagnostics and explicit item-grant test tools. These are not installed with the mod and should not be run on a valued save.

## Credits and permissions

See [CREDITS.md](CREDITS.md). No game executables, regulation files, third-party mod binaries, saves or cheat tables are included. A redistribution/source license has not been selected; no permissive source license is implied. Nexus permission choices remain for the author to set before publication.
