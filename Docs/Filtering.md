# Quick filters in Reader

Reader uses CyUI Core's `TextFilter`, `filterBox`, `filterMenu` and filtered
Inspector directly. No additional UI module, second search engine or Dear ImGui
patch is required. The source lock pins CyUI
`ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3`, which supplies these APIs.

## Where to filter

- File, Edit, Search, Encoding, View, Syntax and Extensions menus have a search
  field. Typing starts there when a menu opens. With an empty query the usual
  nested menus remain; a query reveals matching actions with their group paths.
  The query resets on the next opening of that menu.
- Syntax supports names, canonical IDs, extensions such as `.hlsl`, and special
  filenames such as `CMakeLists.txt`. Its model comes from the common catalogue,
  including registered plugin languages. Accented names are grouped by their
  folded initial; other scripts have an Other syntaxes group.
- Open files matches names and full paths, including accented parent folders.
  Hiding a row never changes the active document, selection, dirty state or Undo.
- Options matches preference labels and their translations. Licenses matches
  notice filenames. Property values and license contents are not indexed.
- Extensions matches plugin names/IDs, command names/IDs, syntax names/extensions
  and option labels. Matching sections expand temporarily and retain their saved
  collapse preference. A plugin-name query also keeps its children visible.
- Search open documents has a separate result filter for source paths, messages
  and positions. It narrows existing results without re-running or changing the
  literal code search. Clearing it restores the original results.

Words can match in any order across a label, its current translation and the
supplied path/context. English names remain searchable in French. Latin case,
accents, ligatures and combining accents are folded; this is not full Unicode
case folding. Each menu/panel owns its query. Escape inside a filter or its clear
button clears only that query.

## Keyboard and edits

Ctrl+F focuses the filter in the focused files/options/extensions/results panel.
With ImGui macOS behavior, physical Cmd+F follows the same route. The helper
does not steal another text input's active edit. In a document, Ctrl/Cmd+F retains
the editor's find/replace behavior; Ctrl/Cmd+Shift+F retains open-document search.
Open menu popups handle their own filter input. The existing SDK log viewer keeps
its own common Diagnostics filter.

Actions retain stable 64-bit IDs, checked states, enabled states and command
parameters. Filtered recent files, syntax modes, encodings and plugin commands
use the same application dispatch path as their unfiltered menu entries.
Disabled results cannot execute. The View menu retains its continuous text-size
slider and offers searchable increase/decrease/reset actions.

Inspector receives the full property model and `InspectorOptions` with the query.
If filtering hides an active row, its draft is cancelled. When a whole plugin
section or settings panel is omitted, Reader cancels its host-owned drafts.
No typed draft is configured, saved or added to document Undo by filtering.
Search-result IDs and source revisions remain intact; cursor navigation still
rejects stale results. Result filtering caches the query, locale and model
revision before reusing prepared rows.

Reader registers its existing English/French catalog with CyUI and synchronizes
the current context locale, so common controls and filter matching use the same
language. Files, plugin data, command IDs and persistence remain app-owned.

## Validation

The `reader_filtering` CTest uses the actual Reader models, common menu renderer,
Inspector and ImGui keyboard/pointer events without SDL initialization or OS
windows. It checks enabled/checked action IDs, a real filtered syntax click,
read-only encoding refusal, translated/accents/path matching, property and section
draft cancellation, popup focus/Escape, result revisions, plugin registrations,
Ctrl/Cmd+F routing, document find and theme/DPI rendering. Existing Reader tests
continue to cover Undo/Redo, language adapters and the six themes.

Windows/MSVC is the executed platform. macOS keyboard behavior is simulated in
ImGui tests; Linux/macOS builds and physical multi-monitor checks remain pending.
