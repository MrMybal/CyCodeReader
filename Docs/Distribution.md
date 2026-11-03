# Windows distributions and public updates

Two Windows 10/11 x64 formats share the CMake project version. Preparation does
not publish releases or access private releases. The updater has no token lookup,
credential storage or authentication. Online checks are **disabled by default**
until the public release build explicitly enables them.

## Editions and storage

The Inno Setup installer is per-user (`PrivilegesRequired=lowest`), normally at
`%LOCALAPPDATA%/Programs/CyCodeReader`. It installs Start menu and optional desktop
shortcuts, Windows uninstall registration, EN/FR setup, application resources,
the native updater and app-local MSVC runtime DLLs. `installed.flag` selects
preferences and optional MCP descriptors under `%LOCALAPPDATA%/CyCodeReader`.
Uninstall preserves these user preferences. It neither changes file associations
nor installs Python/the optional SDK. Installed Reader instances hold an app
mutex checked by Setup; no application is forcibly closed. Setup refuses a
destination containing `portable.flag`, preserving that edition's settings.

The portable ZIP contains `portable.flag`. Extract the complete ZIP to a writable
directory; preferences, bridge descriptors and update cache stay in its `data/`
folder. Development builds without either marker retain their previous adjacent
data directory and cannot update themselves. Neither edition requires Python for
editing or updating. The optional MCP SDK retains its separate setup procedure.
An OpenGL 3.3 driver is required. Native ARM64, Linux/macOS distribution/updating
and clean-machine execution remain outside the locally validated scope.

## Local preparation

```sh
python scripts/fetch_dependencies.py
python scripts/build.py
python scripts/package_windows.py --iscc "C:/path/to/Inno Setup/ISCC.exe"
```

Inno Setup 6.7+ or 7 is a build tool; its official compiler supports portable
deployment. `--portable-only` creates only the ZIP if ISCC is unavailable.
`build.py --deps PATH` reuses a pinned dependency cache, and `--cyui PATH` selects
a clean checkout at the locked revision. No checkout is reset.

Files in `dist/` are `CyCodeReader-<version>-windows-x64-setup.exe`,
`CyCodeReader-<version>-windows-x64-portable.zip` and `SHA256SUMS.txt`.
Packaging uses a clean CMake installation, an allowlist and a package manifest;
development caches, SDK wheels, credentials, preferences, debug symbols and
sources are excluded. Standard **stored ZIP entries without compression** avoid
another archive library or runtime in the updater. ZIP64, archives over 256 MiB
and more than 4096 files are unsupported. `package-files.json` lists managed files.
The original application/document PNG and multi-resolution ICO files under
`icons/` are included. Executable and Setup resources use the application ICO,
the wizard header and native Reader interface use the transparent application
PNG, and shortcuts inherit the executable's first icon. Artwork prompt metadata
is not deployed.
The Git comparison plugin manifest/library are also required package contents.
Git itself is an external dependency discovered in PATH; installation and updates
do not install Git or change repository configuration.

## When the repository is public

1. Make Reader and its required pinned CyUI revision accessible to the build
   runner. CyUI is a separate prerequisite; no private dependency token is added.
2. Keep the CMake version and stable release tag consistent (`0.1.1` / `v0.1.1`).
3. Build with `python scripts/build.py --public-updates`, then package both formats.
   `release-windows.yml` enables this automatically for a public repository event.
4. A public tagged push builds and publishes the complete GitHub Release. Manual
   or private runs preserve workflow artifacts without publishing. Publication
   uses GitHub's ephemeral workflow identity, never an application/local token.
5. Exercise the full online update path with two successive public versions.
   This acceptance is explicitly deferred until the repository is public.

Version 0.1.1 is the first public beta. `packaging/release.json` sets
`prerelease: true`, and the workflow publishes the versioned notes from
`Docs/Releases/v<version>.md`. A future stable release must explicitly set this
field to false. The updater ignores GitHub prereleases, including this beta.

The workflow needs publicly reachable dependencies; local build success does not
establish hosted CI success. The current CyUI pin must be published/accessibly
licensed before another machine can reproduce the build. Public binary recipients
need the exact corresponding GPL sources and dependency notices as in README.md.
Code signing is not configured. If added, sign the application/helper before
packaging and the installer before computing/uploading its final digest.

## Update lifecycle

The Updates panel shows the version/edition, manual checks and optional startup
checks (off initially). Background workers keep network operations off the UI
thread. Only unauthenticated public `/releases/latest` and stable numeric `vX.Y.Z`
versions are accepted. Same/older versions, drafts/prereleases and assets for the
wrong edition are refused. A private repository or absent release produces an
unavailable-public-release message, without authentication fallback.

WinHTTP validates HTTPS certificates, permits only explicit GitHub/CDN redirect
hosts, disables authentication/cookies, bounds sizes/timeouts and supports
cancellation. The asset's exact size and GitHub `sha256:` digest are mandatory.
The helper hashes the package again immediately before applying it. Release
`SHA256SUMS.txt` also supports manual verification.

**Close and install update** follows Reader's existing unsaved-document prompts.
Cancelling a close cancels the handoff. No automatic save or process kill occurs.
After Reader shuts down, its staged helper waits for the parent and up to two
minutes for other instances, then holds an exclusive data-directory lock. New
instances cannot use that copy during the update. Success restarts Reader.

Installed updates run the verified per-user installer interactively at the
existing path. Portable updates check archive structure, CRCs, paths, reserved
Windows names, case collisions, the complete manifest and filesystem links/
reparse points before replacement. A same-volume transaction backs up old managed
files, replaces them and removes obsolete managed files. `data/` and separately
added plugins remain. Filesystem failure restores the previous files; if rollback
itself fails, the surviving backup location is reported for manual recovery.

Downloads/helpers remain in the edition's `data/updates/<unique-id>` cache, with
`update-error.txt` after helper failure. They can be removed when Reader and its
helper are closed. Offline checks cover parsing, integrity, malicious archives,
locks, actual filesystem rollback, managed files and preferences. They do not
establish online, desktop or clean-machine acceptance.
