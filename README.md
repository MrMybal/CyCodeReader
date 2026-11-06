# CyCodeReader

[Documentation](https://mrmybal.github.io/CyCodeReader/#/en/overview) · [Documentation en français](https://mrmybal.github.io/CyCodeReader/#/fr/overview)

<p align="center">
  <img src="https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/icons/CyCodeReader.png?v=0.1.1" alt="CyCodeReader logo" width="160" height="160">
</p>

An independent native C++20 code/text editor built with CyUI, SDL3 and Dear ImGui.
Files open directly without loading CyCode, solutions, language servers, project
tools. CyToolsCore supplies the integrated SDK/MCP companion. Repository: https://github.com/MrMybal/CyCodeReader.

Windows x64 downloads: [GitHub Releases](https://github.com/MrMybal/CyCodeReader/releases).
Version 0.1.2 adds the bundled and online CyDoc manual; see [release notes and known limits](Docs/Releases/v0.1.2.md).

`editor/` is also the shared source library consumed by CyCode: one implementation
of the CyUI editor adapter, lexical/Tree-sitter overlays, icons, snippets and native file I/O.
Changes to that library can benefit both applications without copying code.

## Screenshots

Views from CyCodeReader v0.1.1 using sample C++ documents. Select a preview to
view the full-size image.

[![Code editor with syntax highlighting, tabs and minimap](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/editor.png)](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/editor.png)

| Find and replace | Search across open documents |
| :---: | :---: |
| [![Dedicated find and replace window with minimap match markers](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/find-replace.png)](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/find-replace.png) | [![Search results across open documents with a source preview](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/search-documents.png)](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/search-documents.png) |
| **Side-by-side comparison** | **Git history with inline diff** |
| [![Side-by-side file comparison with syntax colors and inline changes](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/compare.png)](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/compare.png) | [![File revision history and the selected commit diff](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/git-history.png)](https://raw.githubusercontent.com/MrMybal/CyCodeReader/main/Docs/images/git-history.png) |

## Use

Open several files from the command line, the multi-file dialog or drag and drop.
Documents are dockable tabs; drag a tab outside or choose **Move to new window**
for a real native window. Launch another instance to keep separate file sets.

| Shortcut | Action |
|---|---|
| Ctrl+N / Ctrl+O | New document / open files |
| Ctrl+S / Ctrl+Shift+S / Ctrl+Alt+S | Save / Save As / Save all |
| Ctrl+W / Ctrl+Tab | Close / next document |
| Ctrl+F / Ctrl+H / F3 / Shift+F3 | Find window / replace / next / previous match |
| Ctrl+Shift+F | Search every open document, including unsaved buffers |
| Ctrl+G | Go to line |
| Ctrl+P / Ctrl+Shift+P | Open-file / action palette |

The widget provides undo/redo, multiple cursors, folding, bracket matching,
indentation and selection. The default layout shows the minimap, keeps the file sidebar hidden, and uses a
compact icon toolbar with the logo alone. View offers wrapping, whitespace, text size,
six themes and English/French. C/C++/Unreal, C#, HLSL/GLSL, Python, Lua, JSON and
Markdown use shared lexical rules. Rust, Java, JS/JSX, TS/TSX, HTML and CSS use
incremental Tree-sitter parsing, including HTML script/style regions. Syntax works
without installing Rust, Java or Node.js. Project semantic services remain in CyCode.

UTF-8, UTF-8 BOM and UTF-16 LE/BE BOM are detected automatically. Windows-1252
is available explicitly through the Open Windows-1252 command and SDK; it is never guessed. The Encoding menu
converts encodings and LF/CRLF/CR in memory, with an explicit save afterward and
no lossy replacement of unrepresentable characters. Files are limited to 4 MiB.
No-op saves retain original bytes;
edited saves retain the detected newline convention. Disk conflicts preserve the
live buffer, Save As checks existing/open targets, and saves use guarded atomic
replacement. Unsaved documents prompt before close/reload. Preferences are under
data/reader in development/portable builds and `%LOCALAPPDATA%/CyCodeReader/reader`
in installed builds; crash recovery is not implemented.

Windows distribution preparation now provides a per-user installer and a portable
ZIP, plus a native updater for public GitHub Releases. Online checks are disabled
in preparation builds; no token or private-release access is used. See
[distribution and public update preparation](Docs/Distribution.md).

Ctrl+F/Ctrl+H open a dedicated modeless find/replace window with case, whole-word,
selection, wrap and extended-escape options. Search results highlight the text and
mark the minimap/scrollbar. Ctrl+Shift+F searches all open documents with excerpts,
filters and a syntax preview. See [search and default layout](Docs/Search.md).

## File comparison and Git

**Compare** compares two live tabs, a selected disk file or the last saved version.
The shared CyUI diff offers side-by-side/unified layouts, syntax colors, line
numbers, search and change navigation. Unsaved text is included; source documents
and disk files remain unchanged. Outdated snapshots can be refreshed.

The bundled native **Git file comparisons** plugin detects the active file's
repository and adds a searchable **Git** menu: compare the buffer with HEAD or
the index, or inspect staged changes from HEAD to index. The code/line context
menu also exposes file and selection history, following renames, with inline comparisons
against a parent, the current buffer or another selected revision. Split comparisons
share one scrolling canvas so their rows move together. Git must be installed
in PATH; no network access or credentials are needed. See
[comparison behavior, plugin contract and limits](Docs/Comparison.md).

## Quick filters

The File, Edit, Search, Compare, Encoding, View, Syntax, Extensions and Git menus include
CyUI search fields. Filter syntaxes by name or extension (`.hlsl`, `.glsl`), and
filter open files by names or full paths. Options, plugin contributions and
open-document search results have independent filters too. Matching ignores
Latin case/accents and supports multiple words, in English and French.
Ctrl+F focuses a filtered panel's field; in the editor it keeps document find.
Escape clears the query. Filtering never commits a hidden property draft or
changes the live document. See [filter behavior and keyboard routing](Docs/Filtering.md).

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

The locked CyUI commit provides 101 syntax modes, including HLSL/GLSL, file
associations, whole-line Error/Warning colors for `.log` files and syntax engines
contributed by plugins. Reader's Language menu consumes this common registry.
Its richer Unreal/C# adapters and incremental Tree-sitter parsers are preserved.
The normal build uses this catalogue without a development flag.

For further unpublished CyUI work, an explicit opt-in accepts a changed local
checkout without altering the normal revision lock:

```sh
python scripts/build.py --cyui ../CyImgui_Interface --cyui-development --build-dir runtime/build-code-languages-vs
```

The build resolves the exact revision in editor/cyui-source.lock.json. Publish
a local-only CyUI revision before depending on it from another machine; the
validated local cache contains the committed shared Diff correction.

The UI selects **Core, Code, CodeEditor, Commands, Inspector, Diagnostics and Diff**.
Only one ImGui and one TextEditor backend are compiled. Audio, Gallery, demos,
CEF and unused interface modules are disabled. Source/model ownership and the
remaining native compatibility hooks are described in
[the migration notes](Docs/CyUI-Migration.md).

Build outputs remain in runtime/build-vs/Release on Windows, runtime/build elsewhere.
Building runs CTest but does not install, restore old binaries or open an app.
locales/ui.json is deployed beside the build executable. To make a portable folder:

```sh
cmake --install runtime/build-vs --config Release --prefix dist
```

To create both Windows release artifacts with an Inno Setup compiler:

```sh
python scripts/package_windows.py --iscc "C:/path/to/Inno Setup/ISCC.exe"
```

The raw CMake install is a staging folder; `package_windows.py` creates the edition
markers, runtime files and portable manifest. After the repository is public,
`scripts/build.py --public-updates` enables public update checks in release builds.
The release workflow does this automatically for public tagged builds. Online
acceptance will be performed after publication; see [the release guide](Docs/Distribution.md).

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

## License

CyCodeReader and its original shared `editor/` source library are licensed under
[GNU GPL v3 only](LICENSE) (`GPL-3.0-only`). Copyright (c) 2026 Cyberalien;
see [COPYRIGHT](COPYRIGHT). Third-party components retain their own licenses,
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Build folders and CMake installs include LICENSE and COPYRIGHT. Binary recipients
must also have access to the exact corresponding source, including pinned CyUI,
shared editor dependencies and build scripts; private dependency access is not
granted automatically by this notice.

`scripts/verify_reader.py` preserves the hidden desktop acceptance suite and
`scripts/verify_languages.py` covers Reader syntax. They require a deployed binary
selected with CYCODE_READER and do not run during builds. Do not run them while the
existing protection issue remains unresolved. Current evidence and platform limits
are recorded in [Docs/Validation.md](Docs/Validation.md).

## Shared editor development

CyCode consumes this repository at a pinned commit. Its build can select a working
checkout explicitly with CYCODE_EDITOR_ROOT or --editor-root. Only `editor/` is
added as a CMake subdirectory; that does not build or install the Reader application.
Keep the target names and `cycode` namespace stable. See [architecture](Docs/Architecture.md).

## Extensions

The Extensions menu and manager load `plugins/*.json` at startup. Manifests can
contribute commands, typed options and languages. Language packs define extensions,
keywords, types, identifiers and comments. In the expanded CyUI build, they can
also derive from any built-in lexer or provide their own native syntax engine.
The `.cycfg` data pack and `.cynative` native parser demonstrate both paths.
See [the plugin contract](Docs/Plugins.md). Per-plugin panels and a remote
marketplace remain future work.

Native extensions use the versioned C ABI in src/PluginABI.h, exchanging UTF-8 JSON
without C++ objects. examples/sample_plugin.cpp executes a word-count command;
plugins/native-sample.json declares it. Data-only commands can expose built-in text
transforms. Extensions run in-process, are trusted, load once, and require restart
for library/manifest changes. They are not sandboxed or forcibly interruptible.
Options are type-checked and persisted with Reader preferences. Load errors appear
in the manager. See [Docs/Plugins.md](Docs/Plugins.md).

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
SDK task artifacts. See [Docs/MCP.md](Docs/MCP.md) for CLI/service usage and tests.


Local illustrated English/French help is generated and bundled with GUI builds.
See [CyDoc documentation and build instructions](Docs/Documentation.md).
