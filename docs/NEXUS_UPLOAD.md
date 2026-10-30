# Nexus Mods upload checklist

> AI-generated.

## Listing fields

- Game: Elden Ring
- Name: Elden Ring Duplicate Seller
- Version: 0.1.0-beta.2
- Author: Mjolniar (confirm Nexus display name)
- Category: Gameplay (suggested)
- Short description: Press F8 to sell duplicate equipment for trader runes. Keeps 2 weapons and Ashes of War, 1 armor and talisman, and all consumables. Offline beta for game 1.17.1. AI-generated.
- File title: Elden Ring Duplicate Seller 0.1.0-beta.2
- File description: Double-click installer for existing Mod Engine 2 setups. Choose preview or actual selling. Windows x64, executable 2.7.1.0 / in-game 1.17.1 only. Includes DLL, clean configuration, prices and setup guide.
- Requirements: Mod Engine 2; Microsoft Visual C++ 2015-2022 x64 runtime. Randomizer and Seamless Co-op optional, not bundled.
- Installation: extract and run Install.cmd. Do not mark Vortex as supported.

Long description: paste `NEXUS_DESCRIPTION.txt`. Upload the ZIP from `scripts/package_nexus.ps1` only (not a backup or
build folder).

## Open items

- Verify pause-menu/Inventory hint visibility and the 1.5 s return before calling them tested. Keep the beta note until
  then.
- Add a real in-game menu screenshot after that check. None was fabricated.
- Choose Nexus permissions, redistribution terms, donation settings. No license or monetization chosen yet.
- Installer tests cover separate launcher folders, update, uninstall, mode switch, rollback, invalid TOML, package
  corruption, running-game rejection. Not tested on another user's machine.
- Review title, description, archive; publish manually. Nothing has been uploaded.

ZIP format per [Nexus archive guidance](https://help.nexusmods.com/article/117-why-has-my-mod-been-quarantined).
References: [CREDITS.md](../CREDITS.md). No third-party mod binaries bundled.
