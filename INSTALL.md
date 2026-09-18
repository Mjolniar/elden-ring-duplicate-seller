# Installation — 0.1.0-beta.1

## Before starting

Close Elden Ring and back up the save used by your launcher. Use the tested offline setup and executable file version 2.7.1.0 / in-game version 1.17.1. Mod Engine 2 and the Microsoft Visual C++ 2015–2022 x64 runtime must already be installed. This is a manual DLL installation, not a Vortex deployment.

## Install

1. Extract the download's **ERDuplicateSeller** folder into your Mod Engine 2 folder, beside `modengine2_launcher.exe`.
2. Open `duplicate_seller.ini` inside ERDuplicateSeller. Set `sell_value_path=` to the full path of that folder's `sell_values.txt` (no quotes). Example: `sell_value_path=D:\Mods\ModEngine2\ERDuplicateSeller\sell_values.txt`. Alternatively, right-click `Configure.ps1` and choose **Run with PowerShell**; it fills in this path and preserves your other settings. Moving the folder requires updating the path again.
3. Edit the existing `[modengine]` section in `config_eldenring.toml`. Add this DLL to its existing `external_dlls` list. Preserve your other entries; do not add a second `[modengine]` section.

```toml
# Example for a setup with no other external DLLs:
external_dlls = ["ERDuplicateSeller\\ERDuplicateSeller.dll"]
```

4. Launch through your existing offline modded launcher, load a character and stand still.
5. Press **F8**. The default `dry_run=true` previews the sale without removing anything. Read the `native_sale_*.txt` report beside the DLL for the full plan.
6. When satisfied, set `dry_run=false` in `duplicate_seller.ini` and press F8 to sell. Configuration is reloaded on each trigger. A restart is required after changing prices or the displayed hotkey.

The compact menu hint is still awaiting positive visual validation. F8 works independently of hint visibility. Normal results return to the hint after a configured-in-code 1.5 seconds.

## Randomizers and modified trader prices

The bundled table contains 7,202 rows extracted from the installed base-game regulation, SHA256 `766521F9508DE3A3532DF61C45A1C2D93340F1FF7ED8306AB20DF761712CA2AB`. All weapon, armor, talisman and Ash prices match the locally tested randomizer profile. That does not prove compatibility with every randomizer seed or overhaul.

If your active regulation changes item IDs or sell prices, use the source repository's `scripts/extract_sell_values.ps1` with that regulation and matching ParamDef XMLs. It requires Python, .NET 8 and Soulstruct 2.3.2 (installed into a local dependency folder by the helper). Do not use a mismatched table.

```powershell
.\scripts\extract_sell_values.ps1 -RegulationBin 'D:\Mods\MyMod\regulation.bin' -ParamDefsDir 'D:\Tools\Defs' -OutputPath 'D:\Mods\ModEngine2\ERDuplicateSeller\sell_values.txt' -SkipConfigUpdate
```

Restart the game after extraction. This mod never replaces or merges regulation.bin.

## Update / uninstall

Close the game. Back up your configuration before replacing files. To uninstall, remove only this DLL's entry from `external_dlls` and remove its folder. Uninstalling does not restore sold items; use your save backup if you need to undo sales.

## Troubleshooting

- No response: ensure the character is loaded, the game is foreground, the DLL is registered once, and the executable version matches. Read `ERDuplicateSeller.log`.
- Preview only: set `dry_run=false` after checking the report.
- Unknown/missing price: check the absolute path and regenerate from your active regulation. Restart afterward.
- Unsupported layout/version: do not disable the gate; a new game build needs a validated adapter.
- A sale stops: stop testing and preserve the log/report. Do not keep retrying after a fault.

Mod Engine 2 path/list syntax follows its [official configuration example](https://github.com/soulsmods/ModEngine2/blob/main/installer/dist/config_eldenring.toml).
