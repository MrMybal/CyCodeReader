# Validation

## Separation milestone (before modular migration)

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


## Modular CyUI migration — 2026-11-02

The application now resolves official CyUI commit
10e3646482dc8ac587556f475fe2054a88ec5c9e, independently of the local library checkout.
The UI was built from a fresh runtime/build-modular-vs CMake configuration using
Windows x64 / MSVC 2022, the locked ImGui docking revision and C++20.

Executed checks:

- All **9 GUI-enabled CTests passed**. The existing 8 document/core/widget tests
  still pass, including C++/Unreal/C# token roles, six-theme contrast, Tree-sitter,
  snippets, atomic completion, exact tab/UTF-8/final-newline behavior and native
  revision/Undo guards. The 10,000-line highlighting fixture completed in about
  20 ms in this run; that is fixture evidence, not a general performance claim.
- New reader_cyui_modules tests actual Reader views without SDL initialization or
  OS windows: six themes, compact/comfortable density, 100/150/200% DPI, read-only
  plugin JSON, common editor draw/transactions and one-step Undo/Redo, keyboard
  command filtering/disabled commands/Enter/Escape, navigation to a clipped row,
  Inspector pointer/text input and Escape/commit, native option revision changes,
  bounded log filtering/clear and source view revision checks.
- A fresh runtime/core-modular-vs build and **3 core-only CTests passed**.
  That configuration contains no CyUI/ImGui/SDL target or source dependency.
- The existing official MCP acceptance unittest passed against the newly built
  native CLI, with the real CyToolsCore 0.10.1 service and MCP 1.30.0 clients.
  Documents, plugins, encodings, permissions, jobs, stale revisions, explicit saves,
  dirty close and external disk conflicts remain covered.
- Dependency guards accepted the exact clean checkout, rejected a wrong lock
  revision and a modified temporary checkout, and preserved its modified files.
- Generated build projects contain exactly Core, Code, CodeEditor, Commands,
  Inspector, Diagnostics and their single TextEditor backend. One imgui.cpp and one
  build-local TextEditor.cpp are compiled; Audio/Gallery/browser/rendering adapters
  and imgui_demo.cpp are absent. Private evidence is in
  runtime/build-modular-vs/module-audit.json.
- CyUI and TextEditor license notices match the pinned bundled sources.
- Portable installation into runtime/package-modular-check deployed the executable,
  native/data plugin files, SDK facade, EN/FR locales and notices. Its executable
  was not launched.
- Gitleaks 8.30.1 scanned all 98 publishable files, including new untracked sources,
  with zero leaks. Runtime dependencies, binaries, credentials and reports were
  excluded. The redacted local report is runtime/audit/cyui-migration/gitleaks.json.

The original source checkout of CyUI and the CyCode application were not modified.
Full desktop pixel rendering, OS keyboard routing, detached windows and moving
between physical monitors were not re-run. The tests above verify ImGui draw data
and model/input interactions, not an OpenGL/SDL desktop capture. This avoids
repeated app windows and preserves the existing protection investigation.
Linux and macOS execution remain unverified. Exact compatibility/localization
adaptations still required by this CyUI revision are recorded in CyUI-Migration.md.


## Shared syntax catalogue and Logs — 2026-11-02

Local Windows x64 / MSVC 2022 / C++20 build:
`python scripts/build.py --cyui ../CyImgui_Interface --cyui-development --build-dir runtime/build-code-languages-vs`.
This is an explicit development-checkout build; the official source lock remains
unchanged until the expanded CyUI revision is published.

Executed checks:

- **9/9 GUI-enabled Reader CTests passed** after the new catalogue and Logs mode.
  The actual Reader view test selects new modes and its plugin language, opens a
  real temporary `.log` file, checks extension detection and removes plugin
  registrations at destruction. Existing six themes, two densities and
  100/150/200% DPI, Undo/Redo, Tree-sitter, Unreal/C# overlays, snippets and native
  completion tests still pass, without SDL initialization or OS windows.
- The editor integration test registers a declarative `.cycfg` definition and
  loads the compiled sample native DLL; UTF-8 JSON token results color the same
  common CodeEditor. The sample contributes `.cynative` through providerCommand.
- A fresh core-only build at runtime/core-code-languages-vs passed **3/3 CTests**,
  retaining no CyUI, ImGui or SDL dependency.
- The real CyToolsCore/MCP acceptance unittest passed using the rebuilt native CLI.
- CyUI's CodeEditor configuration passed **4/4 CTests**, including **101 syntax
  fixtures**, shader types/intrinsics, file associations, declarative rules,
  inherited word sets, callback validation, registry updates/removal and native
  revision/Undo checks. The HLSL 10,000-line fixture took about 78 ms in one run;
  this is fixture evidence rather than a general performance guarantee.
- Log tests verify whole-line Error/Fatal/Critical and Warning/Warn roles,
  normal INFO and untagged text, subdued verbosity, Unreal/bracketed/JSON formats,
  case-insensitivity, Error priority, complete-word matching, ANSI level prefixes,
  tabs/UTF-8/CRLF preservation, inherited log plugins, editing and Undo.
- Isolated CyUI Core, Code, Diff and DocumentViews configurations built with a
  supplied host ImGui and passed their feature checks with the exact module
  dependency closure. The full Windows build_demo.bat compiled successfully;
  its executable was not launched.
- **9 Unreal packaging unittests passed**, including relocation, Code dependency
  sources, renamed modules and retained vendor hashes. No UnrealBuildTool/engine
  execution was performed.
- Gitleaks 8.30.1 scanned **304 CyUI and 101 Reader publishable files**, including
  new untracked source and vendor files, with zero leaks. Runtime dependencies,
  binaries and private data were excluded. Local redacted evidence is under the
  CyCode runtime/audit/code-catalogue folder.

The portable test artifact is runtime/package-code-languages-check/CyCodeReader.exe
with native/data plugins, locales and all lexer notices. It has not been opened.
Full desktop pixel rendering, OS routing and physical multi-monitor tests remain
pending; the existing protection investigation is preserved. Linux/macOS builds
and execution remain unverified. No push was performed.


## Commit preparation — 2026-11-02

The CyUI syntax commit was exported from its exact staged tree, excluding the
independent localization/gallery edits still in the developer checkout. A fresh
Windows/MSVC build with all modules enabled for validation passed 6/6 CTests
(including all 101 syntax fixtures), and all 9 Unreal packaging tests passed.
Gitleaks found no secrets in the exact export; the commit hook passed as well.
The bundled TextEditor source retains the upstream line endings and updated
vendor hashes, limiting its diff to the actual shared syntax changes.

Reader now pins CyUI commit 37440e74c412bec5b737f344cd40cf5b1f8ff00f in the normal source lock.
The local managed dependency cache was updated from the clean commit, without
changing the developer checkout or pushing either repository. The Reader build
with that clean pinned source is recorded under runtime/build-modular-vs.
