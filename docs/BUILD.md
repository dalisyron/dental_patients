# Build & release notes

## Toolchain

| Tool | Version | Source |
|------|---------|--------|
| MSVC 2022 Build Tools | latest, C++ workload + Win11 SDK | `winget install Microsoft.VisualStudio.2022.BuildTools` |
| CMake | 3.21+ | `winget install Kitware.CMake` |
| Ninja | latest | `winget install Ninja-build.Ninja` |
| Python | 3.12 | `winget install Python.Python.3.12` |
| aqtinstall | 3.x | `pip install aqtinstall` |
| Qt | 6.10.3, `win64_msvc2022_64` | `aqt install-qt windows desktop 6.10.3 win64_msvc2022_64 -O C:\Qt` |
| Inno Setup | 6.x | `winget install JRSoftware.InnoSetup` |

`scripts\setup-windows.ps1` performs all of the above.

## Build flow

```
scripts\setup-windows.ps1     # one-time (~5 GB download)
scripts\run-tests.ps1         # build + ctest
scripts\build-release.ps1     # release build + windeployqt + installer
```

`build-release.ps1` produces `installer\Output\DentalPatients-Setup-<version>.exe`.

## Releasing

1. Bump `project(DentalPatients VERSION X.Y.Z ...)` in `CMakeLists.txt`.
2. Run `scripts\build-release.ps1`.
3. Verify `installer\Output\DentalPatients-Setup-X.Y.Z.exe` opens, installs,
   launches, and that the About dialog shows `X.Y.Z`.
4. Tag the commit:
   ```bash
   git tag -a vX.Y.Z -m "vX.Y.Z"
   git push origin vX.Y.Z
   ```
5. Optionally attach the installer to a GitHub release:
   ```bash
   gh release create vX.Y.Z installer/Output/DentalPatients-Setup-X.Y.Z.exe
   ```
6. Hand the installer to the dentist.

## Why these choices

| Decision | Why |
|----------|-----|
| C++ + Qt Widgets (not QML/Electron/Tauri) | The deployment target is a 2 GB RAM Windows box. WebView/V8/QML each add a runtime that hurts startup and memory. Native Widgets + LTCG produces a fast-starting binary with a small working set. |
| MSVC over MinGW | Standard Windows toolchain; works with the Qt prebuilt MSVC binaries from `aqtinstall`. |
| SQLite + FTS5 | Embedded, robust, supports Persian via `unicode61 remove_diacritics 2`. WAL mode + `synchronous=NORMAL` is the documented "safe across app crashes, fast on slow disks" recipe. |
| Inno Setup over NSIS | Cleaner Unicode/Persian, simpler upgrade semantics via stable `AppId`, modern wizard out of the box. |
| Vazirmatn font | Open-source (OFL), modern, well-shaped Persian glyphs at small sizes. |
| Patient data in `%APPDATA%`, NOT install dir | Uninstall cannot accidentally remove patient records. Per-user storage matches Windows conventions. |
| Soft delete + daily backup with rotation | Belt-and-braces against accidental deletion or corruption. |

## Known cosmetic gaps

- No custom application icon yet (placeholder uses the system default).
- Single light theme only - dark mode is an easy future win.
- Persian translations are inline in the `.cpp` files; they could be moved
  to `.ts` translation files for easier review.
