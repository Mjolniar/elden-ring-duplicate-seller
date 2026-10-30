# Compact menu indicator

> AI-generated.

280 x 24 px click-through panel near the top right; 14 px regular text, muted colors. Idle hint: `F8 | Sell duplicates`
(or the configured hotkey). Completion messages revert to the hint after 1.5 s (up to 100 ms polling delay). A running
operation shows progress until done. Hidden outside menus, even during a sale.

## Read-only menu detection

Frontend HUD-mode enum per [fromsoftware-rs CSFeMan](https://github.com/vswarte/fromsoftware-rs/blob/main/crates/eldenring/src/cs/fe_man.rs):
0 = HUD hidden, 1 = pause menu, 2 = popup menu, 3 = gameplay. Its menu-manager link is checked against the separately
resolved CSMenuMan singleton. No keyboard-toggle guessing, hooks or memory writes.

Pinned executable: file version 2.7.1.0, image size 98621952. Other version or differing anchor = indicator hidden, reason
logged.

- CSMenuMan anchor RVA `0x758ea6`: `48 8B 05 73 69 61 03 33 DB 48 89 74 24 40`; resolves global `0x3d6f820`.
- CSFeMan anchor RVA `0x767213`: `48 8B 0D D6 86 60 03 48 85 C9`; resolves global `0x3d6f8f0`.
- `CSFeMan + 0x18` must equal CSMenuMan; HUD mode = byte at `CSFeMan + 0x78`.
- Requires a loaded player inventory. Visible only in frontend mode 1 or 2, game in foreground.

`scripts/capture_menu_state.py` captures these fields read-only for comparison with the screen. Anchors checked against
the installed executable; live gameplay mode 3 with matching manager pointers confirmed before deployment.

## Scope

Presentation only. F8 and all sale/retention rules unchanged. Previous DLL and config backed up locally. Older v2 release
archives keep the earlier large-banner build.

## Deployment checks (2026-09-18)

- Release build OK. Installed DLL SHA256: `74E400BE30E5ACE05EF56CC2D8060DDC894C8F144414360FF00CCDBACF66AEE0`.
- Fresh game process logged successful indicator init. Indicator absent on startup dialogs, title screen and in
  gameplay. Read-only capture `dist/menu/installed_gameplay.json` confirmed mode 3.
- All carried/key/storage quantities, equipment, inventory count and 124,836,494 runes matched the pre-update snapshot
  (`dist/menu/inventory_preserved.json`).
- A real F8 completed with zero duplicates and zero runes. Before/after checks kept all 348 populated inventory entries
  and 13 equipped records (`dist/menu/f8_unchanged.json`).
- Pending: appearance in pause menu and Inventory; live 1.5 s return to the hint. Automated input did not open the
  menu; a controller check is needed. Modes 1 and 2 come from the referenced layout and are not yet observed here. Menu
  UI is provisional until these pass.
