# Native paged text integration (working sources, 2026-11-13)

Core is pinned to `0541cd0c438bcca507da232e9e1caa2156596910` and CyUI to
`0ba1ba663b4b2bf46d066a32829bc91ea2743cba`. Public v0.1.6 packages predate this
integration, which is included in 0.1.7. The limitations below apply to its
standalone and shared editor profiles. Public 0.1.7 uses the earlier Core
f5e6261bf8acf22d0b3241622ed4294322724fb9; this newer lock is an unreleased source alignment.

## Ownership and threading

`editor/include/cycode/PagedTextEditor.h` and `editor/src/PagedTextEditor.cpp`
are additive graphical source API 1, shared with IDE consumers. The adapter uses
one `cyui::PagedCodeEditor` on the UI thread and one serialized native worker per
open paged document. `CyCode::PagedText` owns LargeText and SpoolStore. Native
immutable snapshots retain transaction history. UI fragments never become a
Document's entire text and no paged storage, atomic writer or Python model is
reimplemented. Imports, lazy line indexing, line scans, syntax, search, clipboard
exports, commits, Undo/Redo and saves execute on the worker. Locks only exchange
small task/completion containers; no native file/index operation runs in draw.
Superseded queued reads/searches coalesce. Streaming scans check cancellation
between chunks. Native import/index/save and the shared whole-snapshot lexer
cannot currently be cancelled while executing. Their UI reports an operation in
progress; closing waits for a native mutation/save to complete, without destroying
the owning worker during publication. Workers join before plugin removal.

Reader chooses this profile for files above the ordinary 4 MiB limit. Small and
untitled documents keep their existing DocumentSession, formats, editor, snippets,
minimap and multi-cursor behaviour. Paged import rejects invalid UTF-8, NUL/binary,
UTF-16 and explicit Windows-1252 without rewriting the source.

## Coordinates, transactions and publication

Document IDs are stable nonzero host IDs outside the ordinary session's range.
Content revisions start at one. Byte offsets, zero-based UTF-16 positions and
tab-expanded visual columns are distinct. Native readLines resolves UTF-16 to
UTF-8; PagedCodeLineBuilder scans complete logical lines in 64 KiB chunks while
retaining only the requested horizontal fragment. Supplementary scalars use two
UTF-16 units; tab stops use CyUI's contract. Initial BOM and untouched CRLF/CR/LF
remain byte-for-byte native data. Inserted newlines follow the first detected
physical newline, scanned in bounded 64 KiB chunks even when the first line is
longer than a page. A CRLF split between chunks stays CRLF; files without a
newline default to LF. Mixed untouched endings remain intact.
Search is literal over physical UTF-8 bytes; use the extended escapes to request
physical CRLF/CR where appropriate. A search for only the LF half of CRLF cannot
represent a distinct selectable newline in UTF-16 line coordinates.

Pages carry request/document/revision guards and include the full byte cost of
fragments, token vectors and row metadata in maxBytes. CyUI additionally validates
all supplied pages. Old completions are discarded; edits map their exact guarded
selection through native offsets. Native edited() produces an immutable candidate;
the host reserves transaction storage, commits and prepares native index metadata
before acceptEdit. Failed preparation restores the preceding snapshot and rejects
the proposal. One proposal is one Undo entry. Undo/Redo restores native snapshots,
issues another revision and restores selections. No-op acknowledgements preserve
the existing redo tail. Cancelled queued input is retained as separate intents
for manual copying/acknowledgement; it is never replayed onto another revision. Rejected replacement proposals also retain their document/revision, exact UTF-16 selection and original text; new editing/saves wait for explicit acknowledgement.

Save calls native LargeText::save; Save As calls native LargeText::saveAs. Both
stream the complete native snapshot. Save As preserves the original and refuses
an existing destination or another open target. Native disk baselines, immutable
payload validation and read-only guards remain authoritative. Pending edits and
unacknowledged cancelled input prevent publication. Failed saves keep the tab and
the close prompt open. No ordinary overwrite confirmation bypasses paged guards.

## Work budgets and commands

- Reads: 64 KiB native chunks; view pages at most 4 MiB including token/row cost,
  4096 rows, 8192 visual columns, 64 KiB per fragment and 65536 tokens per page.
- One insertion: at most 1 MiB after newline conversion. A selection may span
  arbitrarily many pages; a clipboard export is bounded to 4 MiB. Cut publishes
  and checks clipboard text before proposing deletion of the same selection.
- Search: shared native matcher, chunks with overlap and left context, query at
  most 4096 bytes, at most 2000 results, ASCII case folding. Cancellation and revision
  guards apply. Global results retain 64-bit navigation coordinates; int display
  labels saturate. Find/next/previous select actual source ranges. Results are
  marked with the shared warning text role. Global paged results navigate to the
  source rather than copying a complete document into the ordinary preview.
- Ctrl+wheel, selection, typing, navigation, Go to line (64-bit), copy/cut/paste,
  select-all, single replacement, Undo/Redo and guarded full saves are wired.
  Paged replace-all and selection-only search are explicitly unavailable.

These are operation budgets, not a 4 MiB document capacity limit. The native
line index currently rebuilds after each edit; very long individual lines must be
scanned fully to compute complete metrics. No throughput/interactive acceptance
claim is made from a compiler-only build.

## Shared syntax and exact missing contracts

The adapter calls the common CyUI catalogue (including Verse, HLSL/GLSL, log
levels and registered plugin syntax) on a **complete** native snapshot up to
CyUI's 32 MiB source budget, then projects raw UTF-8 spans into fragments. It never
restarts a lexer at a random page boundary inside a comment/string. Above 32 MiB
the view reports unavailable syntax and displays plain text. CyUI needs a shared
streaming/checkpoint lexer API, including plugin state/lifetime, before contextual
coloration can cover arbitrarily large sources. A private partial lexer is not
substituted. The 32 MiB fallback can retain a complete bounded temporary source
and native token snapshot on the worker, never as the widget's document model.

The delivered standalone **TextSession** only registers ordinary Documents. It
does not own paged slots, revisioned paged bridge operations or durable checkpoint
manifests. Therefore paged GUI tabs are not exposed as empty ordinary documents
to SDK/MCP, and no Engine/second MCP is started. A common Core standalone paged
session extension must own document IDs, revisions, histories, guarded operations
and recovery manifests (baseline identity/digest, immutable payload ownership,
format/path metadata, atomic checkpoint publication, validation and reopen) to
provide durable autonomous recovery and one shared paged SDK/GUI session.
LargeText snapshots and SpoolStore::preserve alone are not that contract. Reader
does not copy Engine-private recovery code or leave orphan blobs advertised as
recoverable sessions. Unsaved paged work and cancelled input are volatile.

Paged LSP overlays, project services, folding, multi-cursor, completion/snippets,
minimap, soft wrapping, whitespace decoration and encoding conversion are not
provided by this first CyUI paged editor. Whole-text plugin transforms, comparisons
and history commands keep their ordinary-document profile and refuse paged tabs.
Complex shaping, bidi and grapheme navigation retain CyUI's published limits.

## IDE consumer and evidence

`PagedTextEditor(id, shared_ptr<PagedTextBackend>)` is the injection constructor.
It does not create a LargeText, SpoolStore, session, imported document or Undo
history. `PagedTextBackend.h` is the common presentation contract: state, UTF-16
offset resolution, byte continuations, contextual syntax, prepare/apply/discard,
native Undo/Redo, clipboard extraction, search, whole-document saves and checkpoints.
The Engine remains the unique native owner; returned metadata is authoritative.
Prepare returns an opaque native preview token and changes no content. Apply must
commit exactly one native transaction before returning its new revision/selection;
only then does the view receive acceptEdit. discard is idempotent preview cleanup.

At the locked Core revision, Engine changes.apply/undo returns a staged commit
receipt with transaction, action, historyRevision, documents and
notificationsComplete. An injected backend maps the matching receipt document
directly to PagedTextCommit.status, without a post-commit document reread. A
notification failure is a committed edit with a warning, never a rejected edit.
For RPC transport loss, reconcile the issued plan through changes.status before
resolving the pending proposal; unknown/evicted status does not permit replay.
The backend must keep an unresolved mutation pending rather than report it as
rejected or resend it. In-process preflight exceptions still mean no commit.

Engine Undo/Redo uses changes.historyStatus and changes.historyPrepare, guarded
by expectedHistoryRevision and every affected document identity/revision. Apply
the newly issued preview and use its receipt. Snapshot replay may have edits:[];
it is not an empty operation. The native history retains 256 original transactions
independently of eight preview slots, clears Redo after a new edit and invalidates
affected entries on close/rebind. These Engine contracts belong to the IDE backend;
Reader does not load Engine or add a second history owner.
The backend owns durable recovery, rather than a second adapter checkpoint store.
The standalone constructor retains its existing LargeText mode and explicitly
refuses durable checkpoint publication until Core exposes that common contract.

All backend calls execute on the adapter worker. The host must keep Engine and its
transport alive until the adapter joins; notify external native changes with
refresh(). Each backend operation must guard IDs/revisions at the native boundary.
offset resolves the first requested UTF-16 line position once; subsequent chunks
continue by UTF-8 byte nextOffset (offset + complete UTF-8 text length), avoiding
repeated readLines/nextCharacter scans of a long line's prefix. Shared line builders
still scan complete line metrics with bounded memory. Syntax callbacks receive
the selected common language plus a byte span and must supply contextual spans or
an explicit unavailable message, never a page-local lexical reset.

Standalone Reader resolves the exact locked Core/CyUI pair and refuses a
different or dirty Core override. An embedding IDE supplies its existing clean
CyCode::Core target; the adapter validates its source directory and native/common
API 1, reusing that owner instead of resolving a second standalone Core. The
consumer selects the same CyUI source, JSON/Monocypher cache and one ImGui; the
resolver detects a second Files source owner. No IDE lock or checkout
is silently changed by this Reader task. The IDE backend and adoption are coordinated in its own task; this Reader change does not validate that Engine backend at runtime.

Production builds use BUILD_TESTING=OFF. `editor/tests/paged_text_editor_tests.cpp`
prepares synthetic UTF-8/BOM/mixed-newline, supplementary/tab, 96 MiB, long-line,
stale-read, cross-page edit, Undo/Redo, full-save, Save As, read-only and disk-conflict
fixtures. It is compiler-checked only while native execution is suspended. Manual
acceptance still needs actual clipboard/cut failure, queued cancelled typing,
different themes/densities/DPI and a visible existing instance when authorized.
Linux/macOS builds and native acceptance remain unexecuted for this revision.
