# File comparisons and Git plugin

Reader uses **CyUI::DiffDocument** and **cyui::diffViewer** for text comparisons.
The native CyUI viewer displays
added/removed lines and inline changes with syntax colors, old/new line numbers,
unified or side-by-side layouts, synchronized scrolling, text search and previous/
next change navigation. Comparisons are read-only snapshots; they do not save,
replace, stage or discard source text. English/French labels and the current
theme/DPI apply to the same viewer. Both split columns are painted in one scrolling
canvas, with one vertical scrollbar and a shared horizontal text offset. Wheel,
trackpad, scrollbar drags and change navigation cannot leave one pane an image
behind the other. The shared canvas is implemented in CyUI's Diff module;
Reader's DiffView.cpp only prepares bounded input documents for that API.

## Compare files or open buffers

Use **Compare > Compare files** to choose two open documents. Their current live
text, including unsaved changes, is compared. Untitled and read-only documents
are supported. Alternatively, choose **Select for comparison** on the first tab,
then **Compare with selected tab** on the other. These actions are also in the
editor tab context menu.

**Compare with saved version** compares the current buffer against its last
successful open/save baseline. An external disk edit does not silently replace
that baseline. **Compare with file...** reads a selected disk file as a separate
snapshot without opening another tab or changing either file. Selecting the
active file itself compares it with its current disk contents.

The comparison window can float/dock like other Reader panels. **Side by side**
switches between paired and unified views. The viewer provides **Next change**,
text search and **Find next**; Reader adds **Previous change**. Selecting a current
line can navigate to its guarded live document. Lines from saved/Git snapshots
are never inserted into editable documents.

Changes to an input revision, closing a source or changes to a selected disk
file mark the snapshot outdated. Use **Refresh comparison** to rebuild it. An
asynchronous provider result for a changed/closed/renamed source is rejected.
Changing tabs never applies a result to the newly active document.

This is a text comparison: decoding and newline normalization follow the native
file services. Encoding/BOM/newline-only byte changes are not a binary diff.
Each side retains the 4 MiB text limit, with additional display limits of 100,000
lines and 65,536 bytes per line. The common diff has a bounded work budget; very
large/complex comparisons can use approximate grouping, which is indicated.

## Git file comparisons

The bundled **cy.reader.git** plugin is a real native library and manifest
(`plugins/git.json`, `reader_git_plugin.dll` on Windows). It uses the same
versioned C ABI and plugin manager as other extensions. Git discovery, repository
inspection and blob extraction live in this plugin, outside the editor and
CyCode's IDE Git services. The generic Reader host renders its comparison
contributions and adds a searchable **Git** menu and command-palette actions.
Removing the plugin/manifest and restarting removes those contributions.

Install Git and make its executable available in the application's PATH. Reader
automatically inspects the active saved file and refreshes its context every five
seconds; changing files triggers a new detection. **Refresh repository status**
requests it explicitly. The menu reports the repository/branch, untracked files
and index conflicts. Git ownership/access checks are respected; no safe-directory
exception is created automatically.

The three actions are:

| Action | Left side | Right side |
| --- | --- | --- |
| Compare buffer with HEAD | File at the current commit | Current unsaved buffer |
| Compare buffer with Git index | Staged/index blob | Current unsaved buffer |
| Compare staged changes (HEAD to index) | File at the current commit | Staged/index blob |

Missing historical/index entries are empty text, supporting untracked/new files,
initial commits and staged additions/deletions. Linked worktrees, nested
repositories, detached HEAD, spaces, Unicode and literal pathspec characters are
supported. A conflicted index disables comparisons that require a single index
version; comparison with HEAD remains available. HEAD/index comparisons use the
current path; the history actions below follow prior names. Binary files,
gitlinks and symbolic-link blobs are refused. UTF-8/BOM and BOM-marked UTF-16 are
decoded; explicit Windows-1252 uses the current document's encoding context.

Git commands run on background workers via the existing bounded native process
service and argument arrays. Each child has a four-second deadline and bounded
output. Ambient Git directory/index/pathspec overrides are removed for that
child, filesystem monitor hooks are disabled, and no external diff/textconv
filter or signature program is requested. Local plumbing (`rev-parse`, `ls-files`,
`ls-tree`, `symbolic-ref`, `cat-file`) and read-only `log` are used. Optional writes/lazy fetching are disabled
and allowed transport protocols are empty, so missing objects fail locally.
The plugin does not fetch, push, commit, stage, change branches or request/store
credentials. Git itself remains separately installed, not bundled.

The shared plugin host snapshots options and pins the library before dispatch.
Each plugin's callbacks and returned JSON are serialized by its execution mutex,
while the GUI's document mutex stays available. Context/response/path/revision
validation happens again before display. There is no second document model,
job service or plugin UI backend. Third-party native plugins remain trusted and
cannot be forcibly interrupted through ABI 1; the bundled Git child processes
enforce their own deadlines. Source files remain normal persistent documents.

## File, selection and revision history

Right-click actual code or a line number for clipboard/Undo/search/comparison
actions and the provider menu. An existing selection is preserved when clicking
inside it. A gutter click selects that whole line; code outside a selection uses
the clicked location. Edits are dispatched after the editor finishes drawing.

Git > File history (also the clock toolbar button) opens the file's local history
reachable from HEAD. It follows renames, including Unicode and literal filenames,
and displays commit SHA, date, author and subject with a quick filter. Each page
contains 100 commits; Load older revisions continues through older pages without
a fixed total row cap. Refresh restarts from the current HEAD. Untracked files
without matching committed history display an empty history.

Selecting a commit automatically displays its file diff against its first parent
directly below the history list, including the first selected row on opening.
A draggable divider adjusts the space given to the list and the diff. The inline
viewer retains split/unified modes, syntax colors, line numbers, change/search
navigation, inline highlights and one shared scrolling canvas. A root commit
compares with empty text.

Compare with current buffer and Compare with parent update this embedded view.
Use as left/right revision chooses any two loaded revisions for an inline
comparison. Open comparison window or a double-click can open the larger separate
comparison window. Historical filenames are retained across rename commits.
Historical lines are not treated as current editor offsets when both sides are
committed revisions. No checkout or write is required.

Inline requests run through the same prepared plugin callbacks with at most one
worker in flight. New selections replace the desired request without waiting for
the previous worker on the UI thread. A changed selection, refreshed history,
different source, closed document or a newer unsaved-buffer revision prevents an
obsolete reply from being displayed. Repeated frames reuse the loaded diff.

Selection / line history traces the selected whole lines (or the clicked/current
line) with local Git `log -L`, following their evolution rather than listing every
file change. If unsaved edits inserted/deleted other lines, an unchanged unique
selected block is mapped back to HEAD before tracing. Changed or ambiguous blocks
are rejected with an explanation instead of showing another range's history.
This trace requires a committed HEAD version. New edits while loading invalidate
the range result; select the range again to inspect a different selection.

Both history kinds run as prepared native plugin callbacks off the UI mutex.
They retain the existing child deadline/output bounds, transport prohibition,
encoding checks, local-object-only behavior and per-plugin execution lock.

## Validation and remaining scope

Windows checks include the actual loaded Git library against disposable local
repositories, distinct HEAD/index/disk/unsaved text, conflicts, worktrees, unborn
repositories, encoding and size errors, literal filenames and unchanged index/
disk bytes. Actual ImGui views exercise both layouts, EN/FR, dark/light and
100/200% DPI without an OS window; stale results, source closure, read-only
documents, file-picker cancellation and library pinning are covered.

Full desktop mouse/keyboard acceptance remains pending under the existing
protection investigation. Linux/macOS execution is not established by Windows
checks. Merge editing, stage/unstage buttons, a dedicated blame annotation view
and full repository management remain separate future work.
