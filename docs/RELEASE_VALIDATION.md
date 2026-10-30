# Release validation

> AI-generated.

## Beta.2 installer update (2026-09-18)

- Release build OK; **38 CTest checks passed**, incl. 10 isolated installer scenarios (15.03 s).
- Price loading: adjacent default, relative override, absolute override, missing file. Legacy behaviour kept.
- Installer: install, update, uninstall, keeping other DLL entries/settings/prices, duplicate registration, missing
  list insertion, invalid syntax, corrupted package, path traversal, running-game guard, locked-file rollback, mode
  changes.
- Fixture launchers isolated in ignored `dist/installer-tests`; no live install touched. GUI dialogs and a fresh in-game
  sale not exercised for beta.2.
- DLL SHA256 `2DED89A0DEF5E5B9995E232C80C15488C3A3B5043E4780EBCFF65D9634A7E715`. Sale and keep logic unchanged; only
  price-file resolution changed.
- Installer uses Windows PowerShell and built-in .NET. No Python for normal install; custom extraction is advanced.
- Menu/Inventory hint appearance and on-screen timeout still pending from beta.1. The game install stays on the earlier
  build.

## Distribution preparation (2026-09-18)

- Clean source export built with Visual Studio 2022 / MSVC 19.44, Windows SDK 10.0.26100.0.
- **37 tests passed** in the clean export (10.36 s).
- C++ sources match the installed, live-checked DLL's source (line endings and trailing blank lines may differ).
- Distribution ships the installed DLL, not the fresh build. DLL SHA256
  `74E400BE30E5ACE05EF56CC2D8060DDC894C8F144414360FF00CCDBACF66AEE0`.
- ZIP SHA256 `A96C9BC048B641DE9642CCDD499CBD73A72F7307D2179D747862FC12B197BB57`. 12 files; every payload hash matches the
  manifest. No save, regulation.bin, executable, PDB, backup or runtime log.
- Source and archive scans: no local username, known Steam account ID, credential-token pattern or private-key header.
  Git history not exported.
- Optional Configure.ps1 run in a separate location: changed only the price path, kept preview mode, made one config
  backup.
- Base-game extraction: 7,202 prices. Every weapon, armor, talisman and Ash price matched the catalog-test profile.

## Pending

Pause-menu/Inventory indicator visibility, live on-screen timeout, a real menu screenshot, and the author's
redistribution/Nexus permissions. Not validated on another machine or in multiplayer. Not uploaded to Nexus.
