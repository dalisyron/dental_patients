# Changelog

All notable changes to the Dental Patients application are documented here.
The version numbers follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.1.0] - 2026-08-11

- English is now the default UI language; the full original Persian (RTL)
  localization remains available from the new **Language** menu (restart to
  apply, persisted per machine).
- Layout direction, digit rendering, and field text direction now follow the
  active language; case-number fields stay Latin in both.
- README rewritten around screenshots and an animated demo
  (`docs/media/`, fictional data).

## [1.0.0] - 2026-04-27

Initial release for clinic installation.

### Features

- Persian RTL UI with bundled Vazirmatn font
- Add, edit, search, and delete patients
- Family-name and given-name fields with clinic-specific migration from legacy
  full-name records
- Automatic case-number assignment starting at 6000
- First-run setup for restoring a `.dpbackup` file or starting with an empty
  database
- `.dpbackup` file association, backup creation, and restore flow
- Reuse the running application instance when opening the app or a backup file
- Real-time prefix search across all fields, with normalisation across
  Arabic/Persian Yeh and Kaf and across ASCII/Persian digits
- Soft delete with restore from a recycle-bin dialog
- Daily auto-backup with rotation (last 30 kept)
- Manual backup and export menu items
- Database integrity check on startup with one-click restore prompt
- Keyboard shortcuts for common patient actions
- Inno Setup `.exe` installer with bundled Microsoft Visual C++ Redistributable;
  preserves patient data across upgrades and uninstalls
