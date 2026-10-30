# Installation: 0.1.0-beta.2

> AI-generated. Back up your save. Offline only.

## Before starting

- Close Elden Ring. Back up the save your launcher uses.
- Executable 2.7.1.0 / in-game 1.17.1, offline setup.
- Mod Engine 2 and Microsoft Visual C++ 2015-2022 x64 runtime installed.
- Manual DLL install. Not a Vortex deployment.

## Install

1. Extract the ZIP. Open **ERDuplicateSeller**, run **Install.cmd**.
2. Select **config_eldenring.toml** in the Mod Engine 2 folder you use. **Yes** = actual selling, **No** = preview
   only, Cancel = no changes.
3. Launch through your offline modded launcher. Load a character, stand still, press **F8**.

The installer verifies the package, copies the mod, registers the DLL once, and saves backups in
`.ERDuplicateSeller-backups` in the launcher folder. Other DLL entries and config sections are kept. Refuses while Elden
Ring runs. No admin rights, Python, path editing or price setup needed.

Switch modes: **Settings.cmd** in the installed folder; applies on the next F8. Full preview/sale details:
`native_sale_*.txt` beside the DLL.

The menu hint is not yet visually verified. F8 works regardless. The hint should return 1.5 s after a run.

### Manual install (optional)

Copy ERDuplicateSeller beside your Mod Engine 2 launcher. Add `"ERDuplicateSeller\\ERDuplicateSeller.dll"` to the
existing `[modengine]` `external_dlls` array; keep other entries. The price file beside the DLL is found automatically.
`sell_value_path`: relative to the DLL folder, or absolute. Blank = adjacent `sell_values.txt`.

The archive's default config is preview-only until changed by the installer, Settings.cmd, or `dry_run=false` in the
INI.

## Randomizers and modified prices

Bundled table: 7,202 rows from the installed base-game regulation, SHA256
`766521F9508DE3A3532DF61C45A1C2D93340F1FF7ED8306AB20DF761712CA2AB`. All weapon, armor, talisman and Ash prices match the
tested randomizer profile. Not proof of compatibility with every seed or overhaul.

If your regulation changes item IDs or prices, run the source repository's `scripts/extract_sell_values.ps1` with that
regulation and matching ParamDef XMLs. Needs Python, .NET 8, Soulstruct 2.3.2 (installed locally by the helper). Never
use a mismatched table.

```powershell
.\scripts\extract_sell_values.ps1 -RegulationBin 'D:\Mods\MyMod\regulation.bin' -ParamDefsDir 'D:\Tools\Defs' -OutputPath 'D:\Mods\ModEngine2\ERDuplicateSeller\sell_values.txt' -SkipConfigUpdate
```

Restart the game afterwards. The mod never replaces or merges regulation.bin.

## Update / uninstall

- Update: close the game, run **Install.cmd** from the new download, select the same launcher config. INI and price
  table (incl. custom prices) are kept; replaced files and launcher settings are backed up.
- Uninstall: run **Uninstall.cmd**, select the same config. Removes the DLL registration and moves the folder into
  `.ERDuplicateSeller-backups`; other mods stay. Sold items are not restored; use a save backup.
- A failed install restores changed files and the original launcher config. Unusual TOML layouts are refused; handle
  those manually.

## Troubleshooting

- No response: character loaded, game in foreground, DLL registered once, executable version matches. Read
  `ERDuplicateSeller.log`.
- Preview only: enable selling with Settings.cmd after checking the report.
- Unknown/missing price: check the price file; regenerate from your active regulation; restart.
- Unsupported layout/version: do not disable the gate; a new game build needs a validated adapter.
- A sale stops: stop testing, keep the log and report. Do not retry after a fault.

Mod Engine 2 syntax: [official configuration example](https://github.com/soulsmods/ModEngine2/blob/main/installer/dist/config_eldenring.toml).
