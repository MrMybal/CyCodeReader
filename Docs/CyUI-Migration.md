# Modular CyUI migration

The current source lock is CyUI commit `37440e74c412bec5b737f344cd40cf5b1f8ff00f` from
https://github.com/MrMybal/CyUI. It includes the shared 101-mode catalogue, plugin
syntax engines and Logs mode. The commit is local pending an explicit push;
the validated local dependency cache contains that exact clean revision.
The original migration notes below describe the earlier 10e3646 baseline.
Source remains in the ignored dependency cache; no developer checkout is reset.

## Selected modules and replacements

| Module | Used by | Migration |
|---|---|---|
| Core | Editor adapter and Reader UI | Common themes, fonts, DPI, icons/widgets, layout and notifications; Core.h replaces umbrella includes |
| Code | Shared syntax colors | tokenColor supplies common syntax roles for lexical and parser overlays |
| CodeEditor | Reader tabs and plugin results | Common CodeEditor owns and draws source and read-only JSON; native() connects existing language overlays and revision/Undo adapters; JSON retains selection/copy |
| Commands | Ctrl+P / Ctrl+Shift+P | commandPalette replaces the local popup, filter and selectable rows; app resolves stable nonzero document/action IDs |
| Inspector | Extension options and Reader preferences | Typed properties and per-view drafts replace immediate InputInt/InputText/Checkbox option writes; app validation/configure runs only on commit |
| Diagnostics | SDK/Reader/plugin events and open-document search | Bounded LogBuffer/logView replaces plain SDK error text; diagnosticList presents cached search rows and returns navigation IDs |

Code is enabled transitively by CodeEditor. CMake sets CYUI_MODULES before
add_subdirectory and links explicit targets. The shared editor alone defaults to
CodeEditor; the Reader application adds Commands/Inspector/Diagnostics. Audio,
Gallery, demo, CyUI tests and WebCEF are explicitly disabled. No All selection,
second ImGui, browser, audio engine or rendering adapter is needed. Core file
services still configure without SDL, ImGui or CyUI. ImGui is version checked at
1.92+, and CodeEditor uses the project's C++20 toolchain.

The standalone TextEditor archive dependency and its separate static library were
removed. cycode_texteditor remains a compatibility alias to the single CyUI
backend. imgui_demo.cpp is no longer compiled. CyUI::CyUI is no longer linked.
Licenses for CyUI, its MIT TextEditor backend and the other dependencies remain
in licenses/ and are deployed with the application.

## Ownership and interactions

ReaderCore still owns live documents, native revisions, plugins, persistence and
disk-conflict validation. CyToolsCore still owns MCP identities, jobs and access
control. No service or model is reimplemented by a UI module.

External text edits enter the same native editor through pullLiveDocument, as
one Undo transaction. Exact UTF-8, tabs, final-newline behavior, multiple cursors,
snippets and syntax/semantic overlays retain their existing adapters and tests.
Unsynchronized native changes are refused. Inspector begin/update/cancel phases
stay in drafts; validated commits configure options atomically. A native/MCP option
change advances the supplied revision and cancels active stale drafts. Options
have no new document Undo entries; document Undo/Redo remains in the editor.

Search rows are prepared when searching, retain document revisions and UTF-8 byte
offsets, and are checked before navigating. Read-only JSON result buffers are prepared only
when plugin results change. Logs are capped at 2000 rows / 512 KiB, with common
filtering, follow, scrolling and copy, plus app Copy all / Clear actions.

The custom graphite theme remains application palette data. Duplicated fixed
syntax hue tables have been replaced by common token roles with the existing
cross-background contrast adapter. File-type vector icons remain because they
provide portable per-language badges that Core's general OS font icons do not.
Existing shortcuts, multi-viewport docking and separate application instances
remain in Reader's SDL shell. Preferences now expose compact/comfortable density.

## Exact remaining CyUI gaps

CyUI::CodeEditor exposes native() but does not yet accept a host backend target
or palette transform. Its bundled TextEditor also lacks Reader's existing
transaction-preserving replacements, snippet selection/input/reset/accept hooks,
context tokenizer and revisioned syntax/semantic overlay APIs. Dropping these
would break current behavior. Until a common extension contract is provided,
editor/scripts/prepare_texteditor.py applies those compatibility hooks only to
a build-local copy of CyUI's MIT backend, with checked replacement anchors. CMake
redirects the one cyui_texteditor target to that copy; every consumer sees the
same header and ABI. A small palette transform lets the common draw() preserve
the extra Unreal/token roles. This is an explicit pinned integration adaptation,
not an upstream CyUI patch or an independently maintained CyUI implementation.

CyUI's generic lexer does not cover the existing Unreal reflection distinctions,
plugin-defined lexical rules, Java/HTML/CSS parsers or HTML/JS/CSS injections at
Reader's current fidelity. Their app overlays and Tree-sitter grammars remain.
They share the common colors. CodeEditor snippets/semantic services remain app
adapters rather than language services inside CyUI.

Several module-owned labels (for example log Follow/Copy and diagnostic column
headings) are English literals without a localization callback in this revision.
Reader's own English/French labels remain translated. This limited mixed-language
presentation is documented rather than adding a fork of module implementations.

Reader does not embed in Unreal and has no browser integration: editing Unreal
source does not require the Unreal Editor packaging or Web/CEF modules. It has no
existing diff, timeline, graph, audio or rendered-scene view to migrate.

## Verification

See Docs/Validation.md for executed checks and platform limits. The new CTest
reader_cyui_modules draws actual Reader views in real ImGui frames without SDL
initialization, a GPU backend or native OS windows. It exercises keyboard palette
navigation/scrolling, disabled commands, Enter/Escape, Inspector pointer/text input,
commit/cancel and revision invalidation, common editor transactions, source views
and bounded/filterable logs. The six Reader themes, two densities and 100/150/200%
DPI are exercised. This is draw-data and interaction verification; full desktop
pixel rendering, detached windows and multi-monitor behavior remain separate QA.

Windows/MSVC is the locally tested platform. Linux/macOS remain intended targets
and retain portable sources/build paths, but execution on those systems is not
claimed. No app window, security-setting change, Git commit or push is part of
this migration.


## Shared syntax extension — 2026-11-02

The subsequent local development build uses the new native CyUI Code catalogue:
101 built-in modes, HLSL/GLSL types and intrinsics, whole-line `.log` severity and
a runtime extension registry shared with CodeDocument/CodeEditor/Diff. Reader's
Language menu reads this registry. Plugin manifests register declarative rules,
built-in lexer derivatives or native JSON-token providers. The preceding section
describes the older pinned revision; its generic-lexer/plugin limits no longer
apply to this local catalogue build. Rich Unreal/C# lexical adapters and existing
incremental Tree-sitter parsers remain on the same common editor instance.

No additional UI module is enabled: Core, Code, CodeEditor, Commands, Inspector,
Diagnostics remain the exact production selection. Code adds vendored lexical
engines, public Scintilla interfaces and generated keyword data; it does not add
the Scintilla editor. Native syntax spans have their own palette roles and text
revision. Existing Reader-only hooks remain a checked build-local compatibility
patch. One ImGui and one TextEditor implementation are still compiled.

The default Git lock now pins `37440e74c412bec5b737f344cd40cf5b1f8ff00f`. The normal build consumes
the catalogue from that clean commit; the development opt-in remains available
for future local work. Publish the CyUI commit before Reader when a push is
requested. The initial uncommitted test-package records its own source hashes.
No push was performed.

The separate localization/gallery changes in the developer's CyUI checkout
are preserved outside this syntax commit. Reader's English/French strings and
plugin-owned display names remain intact; some module controls stay English.
Native C++ syntax APIs are covered here; new Python bindings, a remote
marketplace, importing Notepad++ UDL XML/DLLs and forced plugin cancellation are
not implemented. See Docs/Plugins.md for the exact extension contract.
