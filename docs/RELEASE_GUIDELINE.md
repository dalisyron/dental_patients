# Release Guideline

Use this guideline when the user asks to ship, publish, or install an update.
The short key phrases are defined in the repo-root `AGENTS.md`.

## Ship The Update

This creates a new local installer/update file.

1. Make the requested source changes.
2. Run:
   ```powershell
   .\scripts\run-tests.ps1
   ```
3. If tests pass, bump the patch version in `CMakeLists.txt` unless the user
   requested a specific version.
4. Keep the fallback version in `installer\DentalPatients.iss` in sync with
   the `CMakeLists.txt` version.
5. Add a short entry to `CHANGELOG.md` under the new version and today's date.
6. Build the installer:
   ```powershell
   .\scripts\build-release.ps1
   ```
7. Confirm the installer exists at:
   ```text
   installer\Output\DentalPatients-Setup-X.Y.Z.exe
   ```
8. Report the exact installer path and summarize the tests/build result.

Do not commit, tag, push, create a GitHub release, or install locally unless the
user explicitly requested one of those workflows too.

## Publish The Release

This publishes the shipped installer as a versioned GitHub release.

Use the app-visible version from `CMakeLists.txt` for the installer filename,
release title, and git tag.

1. Ensure the ship workflow succeeded for the current version.
2. Read the version from `CMakeLists.txt` and use tag `vX.Y.Z`.
3. Review `git status --short` and include only relevant release files.
4. Commit the release changes with a concise message.
5. Create an annotated tag:
   ```powershell
   git tag -a vX.Y.Z -m "vX.Y.Z"
   ```
6. Push the current branch and tag:
   ```powershell
   git push origin <branch>
   git push origin vX.Y.Z
   ```
7. Create the GitHub release and attach the installer:
   ```powershell
   gh release create vX.Y.Z installer\Output\DentalPatients-Setup-X.Y.Z.exe --title "vX.Y.Z" --notes "See CHANGELOG.md for release notes."
   ```
8. Report the tag, branch, installer asset, and GitHub release URL.

If git or GitHub commands require network/authentication approval, request it
through the tool approval flow. Do not publish if tests or the release build
failed.

## Install Locally

This installs the latest shipped installer on the current Windows machine.

1. Prefer the installer matching the current `CMakeLists.txt` version:
   ```text
   installer\Output\DentalPatients-Setup-X.Y.Z.exe
   ```
2. If that file does not exist and the user only asked to install locally, use
   the newest `installer\Output\DentalPatients-Setup-*.exe` and report which
   installer was chosen. If no installer exists, run the ship workflow first.
3. Run the Inno Setup installer silently:
   ```powershell
   Start-Process -FilePath "installer\Output\DentalPatients-Setup-X.Y.Z.exe" -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CLOSEAPPLICATIONS') -Wait -WindowStyle Hidden
   ```
4. Because the installer uses `PrivilegesRequired=admin`, request elevated
   execution when needed.
5. Report whether the installer completed successfully.

The installer has a stable `AppId`, so re-running a newer installer upgrades the
existing application in place. Patient records live in
`%APPDATA%\DentalPatients` and must remain untouched.
