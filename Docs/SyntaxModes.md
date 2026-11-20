# Syntax selection and Zig/ZON

Version 0.1.10 pins CyUI `e8ccec0a96ed820fc46e86059e010efafcef687e`, which
contains the native Zig and ZON profiles and bundled Lexilla Zig implementation.
The reproducible default build uses a separate commit-named Reader cache. A
modified CyUI checkout is preserved and refused as a normal locked override.
Core and SDK pins are unchanged from 0.1.9. Zig/ZON is included from Reader 0.1.10.

Automatic selection calls `codeLanguageForPath`, including plugin filename and
extension precedence. `.zig`, `build.zig`, `.zon` and `build.zig.zon` select the
appropriate built-in profile unless a plugin overrides their association. Syntax
menus come from `codeLanguages()`. The selected CodeLanguage reaches the common
CodeEditor and search preview, and DiffDocument::setTexts for file/tab/saved and
history comparisons. CyUI internally prepares their CodeDocument with that same
profile. No Reader Zig keywords or lexer are maintained.

Manual IDs use `codeLanguageFromName` and are persisted as strings in
`reader/preferences.json` under `syntaxModes`, keyed by normalized saved paths.
`auto` remains distinct from explicit `plain`, `zig`, `zon` or a namespaced plugin
ID. Legacy display names are normalized on selection. An unavailable plugin ID
remains saved and displays as unavailable/plain rather than becoming automatic.
Existing unknown settings and unavailable plugin options are retained on save.
Edits do not reset the mode; Save As transfers the current choice. Reopening a
saved file after restarting Reader restores its choice. This does not add
unsaved-buffer/session/Undo recovery, which remains unavailable.

Paged rendering uses one complete, bounded CyUI lexical snapshot per native
snapshot/language/catalogue revision (maximum 32 MiB), on the worker. Display
pages project its contextual spans to raw UTF-8 fragment byte offsets, validate
character boundaries and retain native page budgets. No lexer is restarted at a
random fragment boundary. Later pages reuse cached syntax; above 32 MiB the view
reports unavailable syntax and stays plain without loading the whole file for
coloring. An injected backend supplies already contextual fragment-local spans,
which are validated before display. Core storage remains independent of CyUI.

Color changes leave source text, native revisions, selection, scrolling and Undo
untouched. File comparisons preserve view state during recoloring; history and
search preview keys include the language catalogue revision. Themes, DPI and
Ctrl+wheel remain owned by the existing common widgets.

Examples are in [examples/zig](../examples/zig/README.md). Coloring is offline;
ZLS, compilation, debugging and semantic analysis are not included.
