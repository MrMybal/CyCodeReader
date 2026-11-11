# Shared native Core profiles

Reader selects CyCode::TextSession, Documents, TextBridge, Process, Snippets and PagedText through the
canonical cmake/CyCodeComponents.cmake resolver. The exact Core revision and native
source API 1 are recorded in core-source.lock.json. Documents, codecs, search,
text plugins, bounded process ownership, JSON envelopes and loopback transport
are Core implementations; src/ compatibility headers preserve Reader names.
The seven old Reader service implementations and earlier utility copies are
retired. There is one native file/process/session implementation in the graph.

The same BufferState and decode/encode/save primitives are used by the project
Engine and standalone text profile. Reader keeps standalone paths, UTF-16 and
Windows-1252 support, read-only state and editor Undo/Redo. It does not initialize
project/language services or import the full Engine. Syntax remains a CyUI/editor
presentation service. Native source API 1 and graphical editor API 1 are source
contracts; they do not promise a stable C++ binary ABI. Existing text plugin C
ABI 1 remains unchanged and is distinct from project plugin ABI 1/2.

The native dependency cache needs JSON and Monocypher; headless ordinary TextSession does not link the graphical editor. With an explicit development Core, use
`python scripts/build.py --core-only --core-root <CyCode> --deps <dependency-cache>`.
The normal resolver uses its immutable published source revision and preserves
dirty caches. The native service advertises cycode-core/1 through its compatible
cycodereader-native/1 descriptor. Revisions, disk guards, read-only checks, bounded
responses and EOF shutdown are enforced by the shared native owner. CyToolsCore
still owns jobs, identities, permissions and MCP; no parallel scheduler is added.

Reader 671c588 with Core 84e4c78 passed all 17 service/view tests on Windows x64,
Ubuntu x64 and macOS arm64, plus the separate seven-case headless build. Actual
plugins, encodings, guarded edits, IPC/EOF, shared editor/syntax, comparison/history,
filtering and platform/custom snippet completion chords are exercised on runners.
The existing C++ text plugin ABI is unchanged. This does not establish GPU, IME
or physical desktop acceptance. See Docs/Validation.md for exact provenance.
Local application/test execution stays suspended while antivirus removals remain
unresolved. Production compilation and data checks are separate evidence.

# Architecture

The new shared PagedTextEditor adapter and exact missing common contracts are described in [PagedText.md](PagedText.md). Paged tabs own native LargeText snapshots rather than registering empty ordinary Documents in TextSession.

The original application artwork lives in icons/CyCodeReader.png/.ico, with a
separate document variant. The SDL/OpenGL shell owns Branding: SDL's PNG loader
preserves RGBA transparency, one GL texture is shared by Reader's toolbar/welcome
view, and newly detached native windows receive the same SDL window icon.
The texture is released before the GL context. Windows resources embed both
multi-resolution ICOs; Setup uses the application ICO and the PNG in its wizard.
Only the four approved PNG/ICO files are deployed. The shared editor/native core
and selected CyUI modules do not gain a rendering or image-module dependency.

src/ owns Reader windows, document views, menus, dialogs and preferences. ReaderCore integrates the real CyToolsCore SDK/MCP through a private Python
companion; CyCode project/LSP/VCS/build/debug services remain separate. Reader's
optional Git plugin provides read-only file comparisons through the generic
comparison-provider contract, without importing those IDE services.
Native editing does not require Python after compilation. The integrated
CyToolsCore/MCP companion uses its own private Python runtime.

editor/ is a source library with API version 1. CyCode adds only that directory,
using CYCODE_EDITOR_DEPS and CYCODE_EDITOR_BUILD_GUI to select its own dependency
cache and core/GUI mode. Shared target names remain:

- cycode_files: UTF-8 paths, bounded file reads, encoding/newline handling and
  disk-baseline/exclusive-create atomic saves. SDL/ImGui-free.
- cycode_snippets: bounded placeholders, mirrors, choices and UTF-8 spans.
- cycode_editor: widget adapters, themes, lexical rules, Tree-sitter syntax and
  optional semantic display overlays; no language-server process.
- cycode_fileicons: shared palette-based file/folder icons.

Reader and CyCode use these sources directly. The IDE owns LSP completion queries,
semantic analysis and workspace revisions; the library owns their optional widget
integration. The source namespace is preserved to avoid a needless API migration.

scripts/fetch_dependencies.py downloads pinned sources after checksum verification.
editor/scripts/prepare_texteditor.py modifies only a build-local widget copy. No
downloaded source belongs in Git. CyUI is resolved at editor/cyui-source.lock.json from its official Git repository into
runtime/deps/CyUI, or a clean matching CYUI_DIR checkout. Core tests require only
nlohmann/json. CyUI itself is never patched. The existing widget compatibility hooks
are applied to its bundled TextEditor in the build directory. That one backend target
is shared by CyUI::CodeEditor and all native adapters; no second widget is compiled.

Reader owns locales/ui.json, generated by scripts/write_locales.py. The build puts
the locale beside the executable and CMake install deploys both together. Startup
looks only for Reader's locales, without depending on CyCode's app/ directory.
User preferences and reports stay outside version control in data/.
Installed distributions instead use the per-user local application data folder;
portable/development copies retain adjacent data/. Distribution.cpp owns this
choice and the per-copy process lock. GUI preferences and MCP bridge descriptors
share the selected directory. See Distribution.md for installation and updates.

Updates.cpp owns public-only GitHub release parsing, bounded native WinHTTP
downloads and SHA-256 verification. UpdatesUI.cpp owns asynchronous UI state;
no updater operation enters ReaderCore's document API. Online checks require an
explicit public-release build switch, and no credentials are read or persisted.
After normal guarded closure, CyCodeReaderUpdater waits for all instances and
applies the edition-specific package. PortableUpdate.cpp validates/extracts a
bounded stored ZIP and replaces only manifest-owned files with rollback. Installed
updates run the per-user setup. All distribution services remain SDL/ImGui-free.
WindowsCommandLine.h owns the backslash/quote encoding used by native processes,
the updater handoff and Reader restart. updateHelperCommandLine builds and
validates the launch metadata without executing it; trailing directory separators
cannot consume the SHA-256/parent arguments. Older installed launchers need one
manual upgrade before this corrected code handles subsequent updates.


The Core-owned TextSession (ReaderCore source alias) owns shared LiveDocument entries, native revisions, plugin registrations
and document operations under one recursive mutex. GUI tabs reference those same
Document objects. IPC handlers never access ImGui. The GUI holds the lock while
editing, publishes text/revisions, and pulls native updates through LiveEditor as
one widget undo transaction. Unsynchronized local changes are refused. This common
adapter has a headless actual-widget test; full desktop acceptance stays separate.

Bridge binds only loopback and authenticates bounded native JSON requests. It is
private IPC. app/tool.py uses the real CyToolsCore Runtime, schemas, scheduler,
identities, HTTP service and official MCP adapter. Each GUI process starts one SDK
companion with distinct reader/writer credentials and endpoint files. The shared native Process component
owns its hidden child-process tree and bounded logs; closing Reader stops it.
After all dirty-document/save dialogs accept the exit, Reader immediately signals
SDK/update cancellation. The SDL shell hides the main and detached windows before
Reader joins its workers, including on exceptions, and skips further rendering
after accepted closure. Shutdown still joins owned futures before destroying
Core, plugins, ImGui and the OpenGL context. The SDK has no unconditional
two-second wait before its existing process-tree cancellation.
There is no independent Python document model or recreated MCP scheduler.

The Core Documents module performs strict UTF-16 surrogate conversion and explicit Windows-1252
mapping. Document preserves source bytes on no-op saves and tracks pending encoding
and newline changes as dirty metadata. Shared cycode_files still owns bounded reads
and atomic disk-baseline/exclusive-create writes.


Reader tabs own cyui::CodeEditor and retain a reference to native() for lexical/
Tree-sitter overlays, selections and the existing document transaction adapter.
The common renderer calls a build-local palette transform for Reader's extra token
roles. Token colors come from CyUI::Code; only the application's graphite theme
remains an app palette. ReaderCore, disk guards and native revisions are unchanged.

Commands owns command matching, keyboard navigation and popup lifecycle. Reader
resolves stable nonzero command/document IDs after each requested action. Inspector
owns temporary drafts; PluginOptionsView validates and applies only commits through
Plugins::configure, with revision cancellation after native/MCP updates. SDK output
and Reader/plugin errors feed a bounded Diagnostics LogBuffer; the existing process
and SDK lifetimes remain app services. Diagnostic search rows retain source revisions
and are checked again before cursor navigation. JSON plugin output uses a separate read-only
CodeEditor, retaining selection and clipboard access. The Diff module is now
selected for read-only file comparisons; no Web/Unreal embedding, external adapter
or audio module is used.

ComparisonUI owns snapshots of live tabs, native save baselines and selected disk
files. Both comparison views call CyUI's native diffViewer, whose shared scrolling
canvas keeps split rows aligned within the same frame. DiffView prepares bounded
CyUI::DiffDocument inputs. HistoryUI embeds the common viewer below the revision
list. It owns one prepared
preview future and retains a desired request key across selection changes; old
replies are discarded before newer requests start. Keys include history generation,
source identity, revision pair, syntax and the live-buffer revision when used.
The bounded makeDiffDocument preparation serves both views. Source revisions/paths and file stamps
invalidate stale views; no comparison writes a live document. File-context
providers are ordinary native plugins, discovered from manifest metadata. Their
prepared callbacks snapshot options and pin their libraries; native execution is
serialized per plugin outside the live-document mutex, with futures owned by the
Reader view. The bundled Git plugin uses the existing bounded argv process helper
and local Git plumbing; its C ABI and shared plugin permission model are unchanged.
See Comparison.md and Plugins.md for source ownership and contract limits.


Menus.cpp prepares host-owned action trees for CyUI Core's searchable menus.
MenuModel maps stable IDs to the original execute() commands and parameters;
label translations, filtering and reorder do not affect those identities. One
TextFilter per menu/panel keeps queries independent. The existing Reader catalog
is registered with Core for bilingual matching. Focus ownership routes Ctrl/Cmd+F
to a panel filter while preserving document find and native read-only output.

Inspector filtering receives full models and cancels hidden rows. Reader cancels
host drafts when omitting a whole plugin section or panel. Search results retain
their stable navigation IDs and source revisions; their filtered view caches the
query, current locale and model revision. No new service, file search engine,
persistence model, ImGui instance or optional UI module is introduced. See
Docs/Filtering.md for scope and keyboard behavior.
