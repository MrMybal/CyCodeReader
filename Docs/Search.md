# Search and the default editor layout

Code tabs, global-search previews and plugin text views use the bundled JetBrains
Mono Regular 2.304 font. Ctrl+mouse wheel over a text view changes its font size
from 50% to 300%, independently of menus and other views. Ordinary wheel scrolling
is unchanged. Code ligatures are not shaped by the current renderer.

Global-search previews highlight every matching term, including after query,
case/whole-word or theme changes. Global marks ignore a previous local selection
scope. Multiline parenthesis arguments keep matching-pair backgrounds without a
vertical scope guide through the identifiers; brace block guides remain.

Reader starts with the graphite theme, compact toolbar, document tabs, line
numbers, status bar and minimap. The file sidebar, wrapping and whitespace marks
are initially hidden. The toolbar displays the application logo without a name
banner and provides file, clipboard, Undo/Redo, search, comparison, history and
view actions with translated tooltips. Narrow windows omit trailing toolbar
actions; the same commands remain in menus. The first launch after the interface
upgrade applies these layout defaults once. Subsequent view choices are saved;
View > Reset layout restores the defaults.

Ctrl+F opens a modeless, movable/dockable **Find / replace** window. Ctrl+H selects
replacement mode. The common editor's inline search is disabled. Find supports
case matching, whole words, forward/backward navigation, optional wrap, count,
selection-only search and normal/extended input (`\n`, `\r`, `\t`, `\\`). F3 and
Shift+F3 navigate the current document. Search text seeds from a single-line
selection. Escape closes the focused search window; **Clear search marks** clears
the query and its overlays. Matching is literal, with ASCII case folding when
case matching is disabled; regular expressions are not implemented.

Matches receive display-only highlights and thin lines on the minimap. The
scrollbar also shows their positions across the whole document, including matches
outside the currently visible minimap area. Closing Find retains its marks until
the query is cleared. Marks never modify document markers, text, cursors, Undo,
dirty state or disk files, and are invalidated as soon as text changes.

Ctrl+Shift+F opens **Search open documents**. It searches every open buffer,
including unsaved/untitled and read-only documents. Results include highlighted
line excerpts and file/line locations, a path/text filter and a read-only syntax
preview. A click previews a result; double-click or **Open result** activates the
source editor and selects the occurrence. Query/options, source edits and closed
tabs invalidate cached results. Source revision checks prevent navigation through
obsolete offsets. This scope is open documents; recursive folder/project search
is not implemented.

The SDL/ImGui-free search matcher is shared with ReaderCore's native
`search.openDocuments` API. Its byte offsets remain UTF-8 source offsets; the UI
converts them to editor glyph positions. It uses linear-time matching, accepts
up to 4096 query bytes and reports truncation beyond 2000 results. Replace all
refuses a truncated result set and applies one complete, validated in-memory
Undo transaction to the current document. Empty replacements are supported.
Read-only, size and selection-revision guards remain enforced. Saving is explicit.

The build-local TextEditor compatibility preparation adds optional display-only
search spans to the one shared backend. It reuses folding/wrapping-aware selection
geometry and adds minimap/scrollbar ticks, leaving upstream CyUI sources intact.
OS keyboard/window acceptance remains separate from the headless ImGui tests.
