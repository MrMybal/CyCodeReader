# CyCodeReader development

[README](../README.md) · [README français](../README.fr.md) · [Architecture](Architecture.md)

Run commands from the repository root. This guide retains the detailed build,
shared-editor, extension and private SDK setup material from the original README.
For user tasks, use the [English manual](https://mrmybal.github.io/CyCodeReader/Documentation.html#/en/overview)
or [French manual](https://mrmybal.github.io/CyCodeReader/Documentation.html#/fr/overview).
Platform/runtime evidence is maintained separately in [Validation.md](Validation.md).

## Native Core dependency

core-source.lock.json pins the shared native CyCode source API. The build selects
Documents, TextSession, TextBridge, Process, Snippets and PagedText; it does not compile the project
Engine, SDL/ImGui/CyUI or language backends in --core-only mode. Pinned JSON and Monocypher sources
are fetched for that profile. Reader's compatibility headers forward to those
native services; there are no local Document/Search/Bridge implementations.
The application keeps graphical Undo/Redo, preferences and distribution policy.

Use --core-root <CyCode> for an explicit clean checkout at the exact locked revision. Default
builds resolve the immutable lock and refuse modified dependency caches. Native
tests require --run-tests and remain suspended locally during the antivirus
investigation; historical workflows supplied earlier Windows/Linux/macOS evidence. All Actions remain disabled by the owner.

## Core/IDE alignment — 2026-11-13

Reader working sources select published Core `0541cd0c438bcca507da232e9e1caa2156596910`,
native/common source API 1, and retain the existing CyUI source lock. At review
time the published IDE is 1ca2f26d56d7b5dc0dbf4db92e3a08be06ca8e89 and its
independent Core lock is 8b8b7cefef47272871afe7774999f8b838ba1ab1. Reader does
not change that checkout or lock. The modular text profile remains lightweight;
the consumed native delta is Process/Tools, without adding a project Engine,
index, language server or second MCP host. The existing Reader update product and
public 0.1.7 packages stay independent. The local editor-assistance fixes remain.

## Paged source integration

See [PagedText.md](PagedText.md) for the shared API, source pair, worker ownership,
budgets and indispensable Core/CyUI contract gaps. This integration ships in
0.1.7. The existing production build command compiles it with BUILD_TESTING=OFF;
it does not launch a GUI, helper or test. The IDE must explicitly adopt the same
Core/editor/CyUI revisions; its older locks are not changed by a Reader build.

## Building 0.1.7 packages locally

The source version is 0.1.7 beta. For the locally prepared public-update-enabled
Windows packages, use production builds only:

```sh
python scripts/build.py --build-dir runtime/build-0.1.7-vs --public-updates
python scripts/package_windows.py --build runtime/build-0.1.7-vs --output dist/0.1.7 --require-public-updates
python website/build.py --output runtime/showcase-0.1.7 --skip-docs-build
python website/verify.py runtime/showcase-0.1.7
```

These commands compile/package without launching Reader or installing it.
BUILD_TESTING remains OFF. Native execution is still suspended by the existing
antivirus restriction. Keep Actions disabled; publication, push and release are
separate from local preparation. README/showcase source links target the 0.1.7
release. Deploying the showcase is a separate action; do not enable its workflow
without explicit authorization.

## Build

Requirements: Python 3.11+, Git, CMake 3.24+ and a C++20 compiler. Windows uses
Visual Studio 2022 C++; Linux/macOS use Ninja, OpenGL and SDL3 platform development
libraries. CyUI is fetched from its official repository at the exact commit in
`editor/cyui-source.lock.json`, using existing Git credentials if required.

```sh
python scripts/fetch_dependencies.py
python scripts/build.py
```

To use a separate clean checkout at that same locked revision, pass `--cyui PATH`
(or CMake `-DCYUI_DIR=PATH`). A changed revision or tracked local changes are refused;
no developer checkout is reset or updated. For a fresh configuration use
`python scripts/build.py --build-dir runtime/build-modular`.

### Shared syntax catalogue

The locked CyUI commit provides 102 syntax modes, including Verse and HLSL/GLSL, file
associations, whole-line Error/Warning colors for `.log` files and syntax engines
contributed by plugins. Reader's Language menu consumes this common registry.
Its richer Unreal/C# adapters and incremental Tree-sitter parsers are preserved.
The normal build uses this catalogue without a development flag.

For further unpublished CyUI work, an explicit opt-in accepts a changed local
checkout without altering the normal revision lock:

```sh
python scripts/build.py --cyui ../CyImgui_Interface --cyui-development --build-dir runtime/build-code-languages-vs
```

The build resolves the exact revision in editor/cyui-source.lock.json. The locked
revision is available in the official CyUI repository; never substitute an
unpublished dependency in a reproducible release.

The UI selects **Core, Code, CodeEditor, Commands, Inspector, Diagnostics and Diff**.
Only one ImGui and one TextEditor backend are compiled. Audio, Gallery, demos,
CEF and unused interface modules are disabled. Source/model ownership and the
remaining native compatibility hooks are described in
[the migration notes](CyUI-Migration.md).

Build outputs remain in runtime/build-vs/Release on Windows, runtime/build elsewhere.
Ordinary builds keep native tests disabled and do not install or open an app.
locales/ui.json is deployed beside the build executable. To make a portable folder:

```sh
cmake --install runtime/build-vs --config Release --prefix dist
```

To create both Windows release artifacts with an Inno Setup compiler:

```sh
python scripts/package_windows.py --iscc "C:/path/to/Inno Setup/ISCC.exe"
```

The raw CMake install is a staging folder; `package_windows.py` creates the edition
markers, runtime files and portable manifest. For a public release build,
`scripts/build.py --public-updates` enables public update checks in release builds.
The release workflow does this automatically for public tagged builds. Full online
update acceptance remains separate; see [the release guide](Distribution.md).

Normal builds keep `BUILD_TESTING=OFF` and do not execute native tests.
Native tests are deferred while the protection issue is unresolved. After that
issue is resolved, the explicit opt-in for file/snippet tests needs no GUI dependency:

```sh
python scripts/fetch_dependencies.py --core-only
python scripts/build.py --core-only --run-tests
```

The native editor needs no CyCode checkout or Python runtime. The MCP companion
uses a separate private Python/CyToolsCore installation (below). Source archives are
version/hash pinned; no downloaded dependencies, runtimes, personal preferences or
binaries belong in Git.

## Shared editor development

CyCode consumes this repository at a pinned commit. Its build can select a working
checkout explicitly with CYCODE_EDITOR_ROOT or --editor-root. Only `editor/` is
added as a CMake subdirectory; that does not build or install the Reader application.
Keep the target names and `cycode` namespace stable. See [architecture](Architecture.md).

## Extensions

The Extensions menu and manager load `plugins/*.json` at startup. Manifests can
contribute commands, typed options and languages. Language packs define extensions,
keywords, types, identifiers and comments. In the locked CyUI build, they can
also derive from any built-in lexer or provide their own native syntax engine.
The `.cycfg` data pack and `.cynative` native parser demonstrate both paths.
See [the plugin contract](Plugins.md). Per-plugin panels and a remote
marketplace remain future work.

Native extensions use the versioned C ABI in src/PluginABI.h, exchanging UTF-8 JSON
without C++ objects. examples/sample_plugin.cpp executes a word-count command;
plugins/native-sample.json declares it. Data-only commands can expose built-in text
transforms. Extensions run in-process, are trusted, load once, and require restart
for library/manifest changes. They are not sandboxed or forcibly interruptible.
Options are type-checked and persisted with Reader preferences. Load errors appear
in the manager. See [Docs/Plugins.md](Plugins.md).

Edit > Text tools provides full-document line sorting/deduplication, trailing-space
cleanup and ASCII case conversion that preserves other UTF-8 bytes. Ctrl+Shift+F
searches currently open buffers, including unsaved text. Folder search, regex
replacement, macros, arbitrary external run commands, advanced encodings and full
Notepad++ feature coverage remain future work.

## CyToolsCore / MCP

Install the owner's checksum-pinned SDK wheel in this repository's private runtime:

```sh
python scripts/setup_sdk.py --sdk-wheel ../CyToolsCore/dist/cytools_core-0.10.1-py3-none-any.whl
```

The wheel and runtime are not published in this repository. The native Reader starts
one real CyToolsCore service per instance when that private Python is available;
CYCODE_READER_PYTHON can select another compatible interpreter. The CyToolsCore/MCP
panel reports service status and copies separate read-only/editing MCP configurations.
Each instance has private endpoint/credential files; do not share or commit data/.
SDK 0.10.1 and official MCP SDK 1.30.0 are used, not a custom job/MCP implementation.

The SDK operates on the same ReaderCore documents as the GUI: list/read/new/open,
replace/save/close, encoding/newline conversion, search, transformations and plugins.
Expected revisions, read-only state, dirty-close guards and disk baselines are checked.
Read-only identities cannot edit/open arbitrary files or invoke plugins. Saving is
explicit. Plugins require plugins.execute plus workspace.write; opening a supplied
path requires filesystem.read. Source documents are persistent files and are never
SDK task artifacts. See [Docs/MCP.md](MCP.md) for CLI/service usage and tests.


## Documentation

The complete English/French help is generated and bundled with GUI builds.
See [the documentation toolchain](../.CyDocBuild/README.md). Open the generated
`Documentation/Documentation.html` with all of `DocData/` for offline use.

## Licensing and source distribution

CyCodeReader and its original shared editor use [GPL-3.0-only](../LICENSE).
Retain [COPYRIGHT](../COPYRIGHT), [third-party notices](../THIRD_PARTY_NOTICES.md)
and the dependency notices. Binary recipients must have access to the exact
corresponding sources, pinned dependencies and build scripts; private dependency
access is not granted automatically by a license notice.
