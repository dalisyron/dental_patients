# Agent Instructions

This repository has named delivery workflows. When the user uses one of the
phrases below, follow `docs/RELEASE_GUIDELINE.md`.

## Delivery Phrases

- `ship the update`, `ship update`, or `follow the release guideline`:
  finish the requested source change, run tests, bump the patch version unless
  the user gave a specific version, update the changelog, build the installer,
  and report the installer path. Do not publish to GitHub or install locally
  unless the user also asks for that.

- `publish the release` or `publish release`:
  publish the already shipped version. If the current version has not been
  shipped yet, run the ship workflow first. Then commit the release changes,
  create an annotated `vX.Y.Z` tag, push the branch and tag, and create a
  GitHub release with the installer attached.

- `install locally`, `install the update locally`, or
  `install the shipped update`:
  install the latest shipped installer on this machine, replacing the older
  installed version. Prefer the installer matching the current `CMakeLists.txt`
  version. If this phrase is combined with shipping or publishing, run it last.

If phrases are combined, run them in this order:

1. ship the update
2. publish the release
3. install locally

Do not run destructive git operations such as reset or checkout unless the user
explicitly asks for them. Patient data lives outside the install directory and
must not be deleted by build, install, or uninstall workflows.
