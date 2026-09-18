# Compact menu indicator

The indicator is a 280 x 24 pixel, click-through panel near the top-right corner, with 14 pixel regular text in muted colors. The idle hint reads `F8 | Sell duplicates` (or the configured hotkey). Completion messages revert to the hint after 1.5 seconds, with up to 100 ms polling delay. A running operation continues showing progress until it finishes. The indicator remains hidden outside menus even while a sale runs.

## Read-only menu detection

The frontend HUD-mode enum is documented in [fromsoftware-rs CSFeMan](https://github.com/vswarte/fromsoftware-rs/blob/main/crates/eldenring/src/cs/fe_man.rs): 0 hides the HUD, 1 is the pause-menu presentation, 2 is a popup-menu presentation, and 3 is normal gameplay. Its menu-manager link is checked against the independently resolved CSMenuMan singleton. No keyboard-toggle guess, hooks, or game-memory writes are used for visibility.

Pinned executable: file version 2.7.1.0, image size 98621952. An unsupported version or differing anchor hides the indicator and logs the reason.

- CSMenuMan anchor RVA `0x758ea6`: `48 8B 05 73 69 61 03 33 DB 48 89 74 24 40`; resolves global `0x3d6f820`.
- CSFeMan anchor RVA `0x767213`: `48 8B 0D D6 86 60 03 48 85 C9`; resolves global `0x3d6f8f0`.
- `CSFeMan + 0x18` must equal CSMenuMan; HUD mode is the byte at `CSFeMan + 0x78`.
- A loaded player inventory is required. Only frontend mode 1 or 2 allows visibility, and only while the game is foreground.

`scripts/capture_menu_state.py` captures these fields read-only for comparison with the visible game screen. The source layout is research evidence; the exact anchors were checked against the installed executable, and live gameplay mode 3 with matching manager pointers was confirmed before deployment.

## Scope

This change affects presentation only. F8 activation and all sale/retention rules remain unchanged. The previous installed DLL and config were backed up locally. Existing v2 release archives preserve the earlier large-banner build.

## Deployment checks (2026-09-18)

- Release build succeeded. Installed DLL SHA256: `74E400BE30E5ACE05EF56CC2D8060DDC894C8F144414360FF00CCDBACF66AEE0`.
- Fresh game process logged successful menu-indicator initialization. The indicator was absent on startup dialogs, the title screen, and loaded gameplay. Read-only capture `dist/menu/installed_gameplay.json` confirmed frontend mode 3.
- All carried/key/storage item quantities, equipment, inventory count and 124,836,494 runes matched the pre-update snapshot (`dist/menu/inventory_preserved.json`).
- An actual F8 press completed with zero duplicates and zero runes. Independent before/after checks preserved all 348 populated inventory entries and 13 equipped records (`dist/menu/f8_unchanged.json`).
- Pending: visual appearance in the pause menu and Inventory, and the live 1.5-second return to the hotkey hint. Automated menu input did not open the menu; the controller check is still needed. Modes 1 and 2 are based on the referenced layout and have not yet been observed in this installation. The menu UI remains provisional until these positive checks pass.
