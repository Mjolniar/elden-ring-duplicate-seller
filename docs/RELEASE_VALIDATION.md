# Distribution preparation — 2026-09-18

- Clean source export built successfully with Visual Studio 2022 / MSVC 19.44 and Windows SDK 10.0.26100.0.
- All **37 tests passed** in the clean export (10.36 seconds).
- C++ implementation files match the source of the installed, live-checked DLL; only line endings and trailing blank lines may differ in the exported text.
- The distribution includes the installed DLL, not the newly compiled copy. DLL SHA256: `74E400BE30E5ACE05EF56CC2D8060DDC894C8F144414360FF00CCDBACF66AEE0`.
- ZIP SHA256: `A96C9BC048B641DE9642CCDD499CBD73A72F7307D2179D747862FC12B197BB57`.
- The ZIP contains 12 files. Every payload hash matches its manifest. It contains no save, regulation.bin, executable, PDB, backup or runtime log.
- Source and archive scans found no local personal username, known Steam account ID, common credential-token pattern or private-key header. Historical Git commits were not exported.
- The optional Configure.ps1 helper was run in a separate location. It changed only the price path, retained preview mode, and created one config backup.
- The base-game extraction produced 7,202 prices. Every weapon, armor, talisman and Ash price matched the profile used in the completed catalog tests.

## Still pending

Positive pause-menu/Inventory indicator visibility and live on-screen result timeout, a genuine menu screenshot, and author selection of redistribution/Nexus permissions. No validation on another user's machine or in multiplayer is claimed. Nexus upload/publication has not been performed.
