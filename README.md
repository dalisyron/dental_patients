# Dental Patients

A native Windows application for managing dental patient records in Persian.

- **Stack**: C++20 + Qt 6 Widgets + SQLite (FTS5)
- **Target**: Windows 10 / 11, x86-64, Persian (RTL) UI
- **Designed for**: low-end hardware (works comfortably on 2 GB RAM)
- **Storage**: fully offline, single SQLite file in `%APPDATA%\DentalPatients\`
- **Distribution**: single Inno Setup `.exe` installer

## End-user usage

1. Double-click `DentalPatients-Setup-<version>.exe`.
2. Click "Install".
3. Launch from the Start menu or desktop shortcut.

On the first launch the app shows a Persian setup view. Load the clinic's
`.dpbackup` file to populate the exact patient database, or start with an empty
database. After setup, all active data lives in
`%APPDATA%\DentalPatients\patients.db`.

### Updates

Hand the user a newer `DentalPatients-Setup-<new_version>.exe`. Double-click,
"Install". The installer detects the existing version via the stable `AppId`,
upgrades the binaries in place, and **leaves all patient data untouched**.

### Backups

- Automatic: a `.dpbackup` file is written to
  `%APPDATA%\DentalPatients\backups\` on the first launch each day after the
  database has been initialized. The 30 most recent backups are kept.
- Manual: **Tools -> ایجاد پشتیبان** creates a `.dpbackup` file.
- Restore: **Tools -> بازگردانی از پشتیبان...** restores from a `.dpbackup`
  file after confirmation and after creating a safety backup of current data.
- File association: double-clicking a `.dpbackup` launches the app and opens
  the same restore confirmation flow.
- Corruption recovery: if the database is detected as corrupt at startup, the
  app offers one-click restore from the most recent backup.
- CSV: CSV export remains available for Excel/manual inspection, but CSV is not
  used for first-run population or restore.

### Soft delete

Deleted patients move to a recycle-bin table (`patients_trash`) and can be
restored from **Tools -> سطل بازیافت** until purged.

## Developer setup

Requires admin PowerShell on Windows 10/11.

```powershell
# One-time install of MSVC build tools, CMake, Ninja, Python, Inno Setup, Qt 6.
scripts\setup-windows.ps1

# Build + tests
scripts\run-tests.ps1

# Build release + installer
scripts\build-release.ps1
```

After `build-release.ps1` the installer is at:

```text
installer\Output\DentalPatients-Setup-<version>.exe
```

### Bumping the version

1. Edit `project(DentalPatients VERSION X.Y.Z ...)` in `CMakeLists.txt`.
2. Re-run `scripts\build-release.ps1`. The installer filename, the .exe's
   resource version, and the in-app About dialog all read from the same
   single source of truth.

## Layout

```text
.
|-- CMakeLists.txt                 # build entrypoint
|-- src/
|   |-- main.cpp
|   |-- core/PersianText.*         # Persian/Arabic normalisation
|   |-- db/                        # SQLite schema, repository, backups
|   `-- ui/                        # MainWindow + dialogs + table model
|-- tests/                         # Qt Test units
|-- assets/
|   |-- fonts/Vazirmatn-*.ttf      # bundled Persian font (SIL OFL)
|   |-- icons/app.rc               # Windows .exe metadata
|   `-- styles/app.qss             # light theme
|-- installer/DentalPatients.iss   # Inno Setup script
`-- scripts/
    |-- setup-windows.ps1          # one-shot dev environment install
    |-- run-tests.ps1              # build + ctest
    `-- build-release.ps1          # build + windeployqt + Inno Setup
```

## Robustness guarantees

- **WAL journal mode** + `PRAGMA synchronous = NORMAL` - durable across app
  crashes; only an OS-level crash can lose the most recent unflushed write.
- All multi-step writes wrapped in transactions.
- `PRAGMA integrity_check` on every startup; auto-restore prompt if it fails.
- `.dpbackup` is the canonical data transfer and restore format.
- Daily backup with rotation (last 30 kept).
- Restore validates the selected backup and stages it before replacing the
  current database.
- Soft delete: removed records are moved to `patients_trash`, restorable from
  UI.
- Patient names are stored as separate family/given-name fields. Sorting is
  limited to family name (with given-name tie-break) and file number.
- Patient data lives in `%APPDATA%`, never inside the install dir, so an
  uninstall **cannot** remove patient records.

## License

The application source is the customer's. The bundled Vazirmatn font is
licensed under the SIL Open Font License 1.1 (see `assets/fonts/OFL.txt`).
