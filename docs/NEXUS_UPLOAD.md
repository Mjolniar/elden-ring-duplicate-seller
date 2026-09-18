# Nexus Mods upload preparation

## Listing fields

- Game: Elden Ring
- Name: Elden Ring Duplicate Seller
- Version: 0.1.0-beta.1
- Author: Mjolniar (confirm this matches the desired Nexus display name)
- Category suggestion: Gameplay
- Short description: Press F8 to sell duplicate equipment for trader runes. Keeps 2 weapons and Ashes of War, 1 armor and talisman, and all consumables. Offline beta for game 1.17.1.
- File title: Elden Ring Duplicate Seller 0.1.0-beta.1
- File description: Manual Mod Engine 2 installation. Preview mode enabled by default. Windows x64, executable 2.7.1.0 / in-game 1.17.1 only. Includes DLL, clean configuration, prices and setup guide.
- Requirements: Mod Engine 2; Microsoft Visual C++ 2015–2022 x64 runtime. Randomizer and Seamless Co-op are optional, not bundled requirements.
- Installation: Manual. Do not mark Vortex installation as supported.

Paste `NEXUS_DESCRIPTION.txt` into the long-description field. Use the release ZIP produced by `scripts/package_nexus.ps1`; do not upload a local backup or source build folder.

## Remaining author choices and validation

- Verify pause-menu and Inventory hint visibility and the 1.5-second return before presenting these features as fully tested. Keep the beta limitation in the description until then.
- Add a genuine in-game menu screenshot after this check. No image was fabricated to imply a completed check.
- Choose Nexus permissions, redistribution terms and donation settings. No permissive license or monetization selection has been made on the author's behalf.
- Verify installation in another location with the setup helper. A local sandbox setup check is included in the release validation; another user's machine has not been tested.
- Review title, description and archive; then publish manually. No Nexus page or upload has been submitted by this preparation task.

ZIP format follows the [Nexus archive guidance](https://help.nexusmods.com/article/117-why-has-my-mod-been-quarantined). The project uses research references listed in [CREDITS.md](../CREDITS.md); no third-party mod binaries are bundled.
