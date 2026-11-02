# Validation — separation milestone

This repository was extracted from the locally developed CyCodeReader and the
actual shared editor library in CyCode. Historical Windows desktop acceptance
covered editing/undo/redo, save guards, six themes, two independent processes and
real detached native windows, with OS hidden flags checked. That history does not
establish graphical acceptance of the newly separated build.

Current Windows x64 checks passed:

- Gitleaks scanned all 92 publishable source files, including initial untracked
  sources: zero leaks. Runtime environments, downloaded sources, SDK wheels,
  credentials, reports and binary outputs are excluded from Git.
- Independent Reader build from its own checkout and own hash-pinned dependency
  cache, with CyUI supplied separately. No CyCode source checkout is a build input.
- All eight GUI-enabled CTests passed without an OS window. They include file
  save/conflict/encoding guards, actual native/data plugin loading and commands,
  typed options, native revisions/search, snippet behavior, shared syntax/colors,
  and the real-widget Reader integration for plugin keyword/type/comment roles,
  native edit synchronization, one-step undo and unsynchronized-edit refusal.
- The core-only build and all three CTests passed without CyUI/SDL. UTF-8/BOM,
  UTF-16 LE/BE with surrogate pairs, strict Windows-1252 conversion, no-op byte
  preservation, pending encoding/newline metadata and malformed/lossy refusal
  are exercised by native document tests.
- A real shared CyToolsCore 0.10.1 HTTP service and two official MCP 1.30.0 clients
  passed schema, live document, native/data plugin, encoding metadata, read/write
  permission, private-job isolation, stale revision, explicit save, dirty-close
  and disk-conflict acceptance. Unicode Windows CLI file arguments are exercised.
  The private SDK wheel hash is verified by setup_sdk.py; it is not published.
- CMake portable install deployed the executable, locales, SDK facade/manifest,
  native plugin/library manifests and license notices into a private test folder.
  The executable was not launched. This is a resource/package smoke check, not
  full desktop or clean-machine acceptance.
- CyCode compiles against the extracted editor module; its 36 native and 20
  official MCP regression tests passed. Reader's document test now belongs to
  this repository and is no longer counted in CyCode.

No GUI launch or protection-setting change is performed by
the build script. The existing protection-related GUI investigation remains open;
no old quarantined CyCode binary is restored. Linux/macOS execution and a public
binary installer/package remain unverified.

Reader has no language-server completion, project semantic colors, debugger, Git,
folder-wide search, macro recorder, remote extension marketplace or unsaved-document
crash recovery. Plugin languages currently use C++/JSON lexical bases; arbitrary
external parser engines and per-plugin UI panels remain pending. Native plugins
are trusted in-process code and cannot be forcibly cancelled or sandboxed. Full
Notepad++ feature coverage and the newly expanded desktop visual flow remain pending.
