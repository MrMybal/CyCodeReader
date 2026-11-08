# Validation

## Shared Core extraction — 2026-11-08

Reader 671c588b924f9da2655843f64615b6b6bc7076e6 pins Core
84e4c7831afeab27406a958a957db23fe005eae8. All 17 native/service/view CTests pass
on Windows x64, Ubuntu x64 and macOS arm64, and the separate seven-case headless
graph also passes. Exact run:
https://github.com/MrMybal/CyCodeReader/actions/runs/42493128377

The suite exercises actual shared documents/codecs, data/native plugins, revision
and disk guards, real CLI/loopback EOF cleanup, Git environment isolation,
comparison/history, editor Undo/Redo, syntax, snippets, filtering and CyUI views.
Catalogue assertions now match the pinned 102 built-in modes including Verse.
Snippet completion follows the actual backend chord; tests cover default/custom
chords and macOS physical-to-logical modifier mapping. JSON/string conversions
and translated varargs were corrected for portable C++ behavior.

Windows production Reader and minimal native component builds also pass locally,
with tests OFF. No native app, CLI, updater, debugger or test executable was run
locally. Source/data checks, local compilation and runner execution are separate
evidence. Headless view loops do not prove OS windows, GPU drivers, IME, actual
multi-monitor behavior or every distribution package. Original active Reader
sources remain untouched; these changes are on codex/core-common.

## Shared Verse syntax — 2026-11-06

Windows x64 Release Reader compiles against the exact committed CyUI Verse
revision, with native tests OFF. Both Reader and CyCode use the shared native
scanner and catalogue. The actual scanner passed 32 compiler-evaluated assertions;
catalogue/plugin/editor test bodies were checked by MSVC /Zs only, not executed.
Reader source checks passed (24 Python and 14 JSON files); its paired EN/FR
manual regenerated with validated anchors, assets, translations and references.
No GUI, SDK/MCP or native test executable was launched. Runtime plugin/Undo,
UI/theme/DPI and Linux/macOS acceptance remain pending. Verse compilation and
an official UEFN language-service integration are not included.

## File comparison and native Git plugin — 2026-11-03

Windows x64 / MSVC 2022 Release compilation and all **15 CTests passed** with the
locked CyUI revision. The actual CyUI Diff module is selected; no library checkout
or vendored editor source was changed. A separate core-only build passed **6/6
CTests** without SDL/ImGui/CyUI. The existing real CyToolsCore HTTP/official MCP
client acceptance test also passed with the updated native CLI and the existing
ABI 1 sample plugins.

The native Git DLL passed **8 independent local-repository scenarios**: distinct
HEAD/index/disk/unsaved buffers, Unicode/spaces/literal pathspecs, untracked and
staged-deleted files, unborn repositories, linked worktrees/nested repositories,
detached HEAD, real conflicting branches/index stages, UTF-16/Windows-1252,
binary/oversized refusal, ambient Git directory/index isolation, missing Git and
disabled external filter/fsmonitor execution. Index and working-file bytes remain
unchanged by plugin calls. Git 2.45.1.windows.1 was exercised locally.

The real ImGui Reader comparison view is exercised without SDL video or OS
windows: two unsaved tabs, saved baseline versus an external disk edit, selected
file/picker cancellation, line additions/removals, source closure, stale results,
refresh, read-only documents, the common Core plugin API, menu filtering in French
and native library lifetime pinning after owner destruction. Both diff layouts
render in Graphite/light-grey, EN/FR and 100/200% DPI. The standard module tests
retain their six-theme/density/DPI and branding checks.

See Comparison.md and Plugins.md for use, bounds and callback ownership. No GUI
application/installer was launched, no user repository/index was modified, and
no fetch/push/private-release request was performed. Commits/merges/worktrees in
the tests belong exclusively to temporary fixtures. Full desktop interaction
and Linux/macOS execution remain unverified under the existing protection issue.

## Application branding — 2026-11-03

The supplied PNG/ICO artwork is integrated without changing the original files.
Windows Release compilation and all **13 CTests passed**. The module test decodes
the real logo with SDL's PNG loader, checks transparent/visible pixels and draws
the branded Reader views in six themes, two densities and 100/150/200% DPI without
initializing SDL video or opening OS windows.
Offline PE resource inspection confirms that CyCodeReader, its updater and the
compiled installer contain the original application icon's **10 resolutions**,
with matching frame payloads. The rebuilt portable ZIP contains all four supplied
PNG/ICO files byte for byte; its manifest, CRCs and release SHA-256 sums pass.
Setup's small wizard image uses the same transparent PNG. The GitHub README uses
a relative image reference, so it renders from the repository without an external
image host. The application/installer were not launched, and no push was made.
Desktop/installer pixel acceptance remains pending with the existing protection
investigation; the offline checks above do not establish that acceptance.

## Public distribution preparation — 2026-11-03

Windows x64 / MSVC 2022 Release compilation passed with the clean locked CyUI
revision and the existing checksum-pinned dependency cache. All **13 CTests
passed**, including two independent offline distribution checks and a real
ImGui update-panel test without SDL initialization or native OS windows.
The latter covers English/French, graphite/light-grey, 100/200% DPI, unchanged
unsaved documents, guarded closure and cancellation of the update handoff.
Native distribution checks exercise numeric versions, exact edition/asset
selection, absent/malformed SHA-256 metadata, untrusted API URLs, traversal,
Windows device names, case collisions, corrupt/compressed archives, manifest
mismatch, real instance locks, actual filesystem rollback, managed-file removal
and preservation of preferences/custom plugins. No GitHub request is involved.

The Windows portable ZIP and per-user installer compiled successfully. The
packager verifies installed resources, app-local MSVC DLLs, ZIP CRCs/manifest,
file allowlists and final SHA-256 sums. The installer was compiled with the
official digitally signed Inno Setup 6.7.4 compiler deployed in portable mode;
the Reader installer/application was not launched or installed.
`CYCODE_READER_PUBLIC_UPDATES` is **OFF** in these preparation binaries. No local
GitHub token, credential read or private-release test is implemented/performed.
See Distribution.md for enabling the public build and the exact release names.

Online download/redirect behavior, hosted release CI, installed upgrade/uninstall,
full desktop interaction, code signing and clean-machine acceptance remain
unverified. Full update acceptance will be performed with successive public
releases after the repository becomes public. The locked CyUI source must also
be reachable by the release runner. No repository visibility change, commit,
push, release publication or security-setting change was performed here.

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

Reader has no language-server completion, project semantic colors, debugger, full Git repository workflows,
folder-wide search, macro recorder, remote extension marketplace or unsaved-document
crash recovery. The new native Git plugin supplies read-only file comparisons,
not repository management. Plugin languages currently use C++/JSON lexical bases; arbitrary
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


## CyUI quick filtering — 2026-11-02

The normal Windows/MSVC build uses the exact clean CyUI filtering revision
ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3. Core already provides the filter APIs;
no optional module, duplicate widget or library source patch was added.

All 10 GUI-enabled Reader CTests passed in runtime/build-modular-vs, including
new reader_filtering coverage of the actual menu renderer and Reader models:

- Stable action IDs across enabled states, translation and recent-file ordering;
  a pointer click selects the requested shared syntax, checked state survives,
  and disabled read-only encoding actions cannot run.
- English/French multi-word matching, accent folding, accented parent paths,
  plugin syntax extensions and translated submenu breadcrumbs; accented syntax
  names remain in their alphabetical group.
- Menu popup typing/focus and Escape clearing, Ctrl+F/Cmd+F in a filtered panel,
  Ctrl+F in a document, and Ctrl+Shift+F opening search across open buffers. The
  latter has priority over the native widget's conflicting Find All shortcut.
- Hidden property drafts, omitted plugin sections and closed panels cancel
  without configuring options or changing live document Undo/dirty state.
- Result-filter cache invalidation and stable 64-bit navigation IDs, including
  source paths containing literal ## characters. Code-search query semantics
  and stale-source revision guards are unchanged.
- The Open dialog preserves contributed language registrations. Actual views
  render in graphite/light-grey and 100/200% DPI. Existing module tests retain
  their six themes, two densities and 100/150/200% DPI coverage.

These tests create no OS windows and do not initialize SDL. macOS key behavior
is simulated in ImGui, not evidence of a macOS build. Linux/macOS execution,
physical display movement and desktop pixel acceptance remain unverified.
The rebuilt portable test folder is runtime/package-filtering-check; its
executable is not launched by the build. Gitleaks audit evidence is recorded
under the CyCode runtime/audit/reader-filtering folder. No push was performed.

## Reader interface, search and Git history — 2026-11-03

The Windows x64/MSVC GUI-enabled build passes **16/16 CTests** without SDL
initialization, GPU rendering or native application windows. The new experience
test draws the actual Reader in EN/FR, graphite/light-grey and 100/200% DPI. It
checks the default minimap/sidebar configuration, dedicated find, literal and
extended searches, whole-word/case behavior, UTF-8 positions, selection bounds,
empty/read-only replacement guards, one-step Undo, unsaved open-document scope,
stale/closed result invalidation, display-only search spans and actual code
right-click popup input. A near-match 4 MiB fixture exercises the linear matcher.
The diff test verifies exactly one scrolling canvas and input over both columns.

The actual loaded Git DLL passes **11 disposable-repository scenarios**, including
rename-aware metadata and old-path blobs, root/parent/two-revision comparisons,
line history, unchanged unsaved-range mapping, rejected changed ranges and a
104-commit two-page traversal with no duplicates. Existing worktree, conflict,
encoding, binary, size, literal filename and index/disk preservation checks pass.
Reader's actual history panel and asynchronous stale-selection guards are tested.

Optional CPU raster captures of real ImGui draw data were inspected for the
default editor, find/replace, search results/preview, context popup, Git history
and aligned comparison. They use an explicit legacy-test glyph preload for the
icons/French punctuation loaded dynamically by the desktop renderer. These are
headless layout evidence, not desktop/OpenGL acceptance. No app/installer was
launched; the existing protection investigation remains respected. Linux/macOS
execution, physical monitor movement and interactive desktop acceptance remain
unverified. Public update checks stay disabled and no credential is embedded.

## Embedded history file diff — 2026-11-03

The Windows GUI-enabled build passes **16/16 CTests** with the embedded history
diff. The actual Git/ImGui fixture verifies automatic first-row/root diffs, a
second revision against its parent, inline buffer and two-revision comparisons,
separate-window expansion, rapid selection with a previous worker still running,
discarded obsolete buffer replies and source closure. Rendering is exercised in
EN/FR, graphite/light-grey and 100/200% DPI. Live text, selection and Undo snapshots
remain unchanged by viewing the diff. The common bounded diff preparation is
shared with the standalone comparison window; no CyUI checkout or plugin ABI
change is involved. A CPU raster capture of the actual embedded view was inspected.
These checks create no OS windows. Desktop/OpenGL and other platforms remain
unverified under the existing acceptance constraints. Packages are regenerated
with public updates disabled; no publication, installation or app launch is made.

## Shared CyUI Diff correction — 2026-11-03

Reader pins clean CyUI commit a228501d9418550ce68d2d67b70cebbb8fa3ee2a.
ComparisonUI and the embedded history preview call native cyui::diffViewer;
the Reader-only renderer has been removed. DiffView retains source validation
and bounded document preparation. The CyUI API/state layout is unchanged.

The Windows/MSVC all-module CyUI build passes **8/8 CTests**. The new native
regression covers one scrolling child, wheel input over either column, vertical
scrollbar dragging, horizontal offsets with fixed gutters, row navigation,
explicit scroll restoration, hunk focus and empty documents. It draws split and
unified views across five themes, both densities, EN/FR and 100/150/200% DPI.
build_demo.bat succeeds; all ten hidden DX11 gallery captures (five themes and
two densities) pass and their comparison sheet was visually inspected.

The normal GUI-enabled Reader build with that exact clean commit and development
mode disabled passes **16/16 CTests**, including the actual comparison/history
panels and shared canvas input. Optional CPU captures use the rebuilt library.
Reader and its installer were not launched. Public updates stay disabled;
the commit is local, with no push or credential added. Linux/macOS and interactive
Reader desktop acceptance remain unverified. The independent Python CyUI renderer
is outside this native C++ change.

## Public beta v0.1.1 — 2026-11-03

The Windows x64/MSVC Release build passes **16/16 CTests** at version 0.1.1
with CYCODE_READER_PUBLIC_UPDATES=ON and development CyUI mode disabled. The
normal dependency resolver uses the clean exact source pin; CyUI commit
a228501d9418550ce68d2d67b70cebbb8fa3ee2a is now available on public CyUI main.
The versioned release notes disclose the remaining desktop/clean-machine,
installation/uninstallation and online update acceptance work. The beta is a
GitHub prerelease and is intentionally ignored by the stable update channel.

The release workflow YAML and its publication Python block pass local syntax
validation. The workflow requires matching numeric tag/CMake versions, a boolean
prerelease profile and versioned notes. Tagged public builds reconstruct and test
the packages on Windows/MSVC before uploading the installer, portable ZIP and
SHA256SUMS.txt. Reader and its installer were not launched during preparation.
