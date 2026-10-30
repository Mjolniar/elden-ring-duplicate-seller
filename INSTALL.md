# Installation — 0.1.0-beta.2

## Before starting

Close Elden Ring and back up the save used by your launcher. Use the tested offline setup and executable file version 2.7.1.0 / in-game version 1.17.1. Mod Engine 2 and the Microsoft Visual C++ 2015–2022 x64 runtime must already be installed. This is a manual DLL installation, not a Vortex deployment.

## Install in three steps

1. Extract the ZIP. Open **ERDuplicateSeller** and double-click **Install.cmd**.
2. Select **config_eldenring.toml** in the Mod Engine 2 folder you actually use. Choose **Yes** for actual selling or **No** for preview only. Cancel makes no changes.
3. Launch through your usual offline modded launcher. Load a character, stand still and press **F8**.

The installer verifies the package, copies the mod, registers its DLL once, and saves backups under `.ERDuplicateSeller-backups` in the launcher folder. It preserves other DLL entries and other configuration sections. It refuses to run while Elden Ring is open. No administrator access, Python, path editing or separate price setup is needed for the normal installation.

To switch modes later, double-click **Settings.cmd** in the installed ERDuplicateSeller folder. The change applies on the next F8 press. Full preview/sale details appear in `native_sale_*.txt` beside the DLL.

The compact menu hint is still awaiting positive visual validation. F8 works independently of hint visibility. The indicator is designed to return to its hotkey hint after 1.5 seconds.

### Manual installation (optional)

Copy ERDuplicateSeller beside your Mod Engine 2 launcher and add `"ERDuplicateSeller\\ERDuplicateSeller.dll"` to the existing `[modengine]` `external_dlls` array. Preserve your other entries. The default price file is located automatically beside the DLL. `sell_value_path` accepts relative paths from the DLL folder or an absolute override. A blank setting also selects the adjacent `sell_values.txt` for the native seller.

The archive's default configuration remains preview-only until changed by the installer, Settings.cmd, or `dry_run=false` in the INI.

## Randomizers and modified trader prices

The bundled table contains 7,202 rows extracted from the installed base-game regulation, SHA256 `766521F9508DE3A3532DF61C45A1C2D93340F1FF7ED8306AB20DF761712CA2AB`. All weapon, armor, talisman and Ash prices match the locally tested randomizer profile. That does not prove compatibility with every randomizer seed or overhaul.

If your active regulation changes item IDs or sell prices, use the source repository's `scripts/extract_sell_values.ps1` with that regulation and matching ParamDef XMLs. It requires Python, .NET 8 and Soulstruct 2.3.2 (installed into a local dependency folder by the helper). Do not use a mismatched table.

```powershell
.\scripts\extract_sell_values.ps1 -RegulationBin 'D:\Mods\MyMod\regulation.bin' -ParamDefsDir 'D:\Tools\Defs' -OutputPath 'D:\Mods\ModEngine2\ERDuplicateSeller\sell_values.txt' -SkipConfigUpdate
```

Restart the game after extraction. This mod never replaces or merges regulation.bin.

## Update / uninstall

Close the game. Extract a new download and run its **Install.cmd**, selecting the same launcher configuration. Updates preserve the installed INI and price table, including any custom prices. Existing mod files and launcher settings are backed up before replacement.

Run **Uninstall.cmd** and select the same launcher configuration to remove this mod's DLL registration. Its folder is moved into `.ERDuplicateSeller-backups`; other mods remain registered. Sold items are not restored by uninstalling. Use a save backup to undo sales.

If installation fails partway through, the installer restores changed files and the original launcher configuration. An unusual TOML layout is refused before installation and can be handled manually.

## Troubleshooting

- No response: ensure the character is loaded, the game is foreground, the DLL is registered once, and the executable version matches. Read `ERDuplicateSeller.log`.
- Preview only: use Settings.cmd to enable selling after checking the report.
- Unknown/missing price: check the price file and regenerate from your active regulation. Restart afterward.
- Unsupported layout/version: do not disable the gate; a new game build needs a validated adapter.
- A sale stops: stop testing and preserve the log/report. Do not keep retrying after a fault.

Mod Engine 2 path/list syntax follows its [official configuration example](https://github.com/soulsmods/ModEngine2/blob/main/installer/dist/config_eldenring.toml).
