---
id: paged-text
title: Large UTF-8 files
description: Edit large files through native storage and asynchronous pages.
section: Using Reader
order: 35
parent: editing
revision: 3
---

## Open and navigate {#paged}

Version 0.1.7 opens files above 4 MiB in **Paged UTF-8** mode.
Public v0.1.6 installers predate this feature. Import and line indexing run in the
background; the tab announces progress or an import error. Invalid UTF-8, binary,
UTF-16 and explicit Windows-1252 are refused without converting the source. Small
files and new drafts retain their ordinary editor and encoding choices.

Use selection, typing, Undo/Redo, Ctrl+C/X/V, Ctrl+A, Ctrl+G and Ctrl+wheel.
Go to line accepts 64-bit line numbers. Horizontal fragments also handle very long
lines. Document size is independent of the bounded page held by the view. Native
indexing currently rebuilds after an edit; very large edits/lines can take time.
Wait for an announced native operation to finish before closing or saving.

## Search and clipboard {#search}

Ctrl+F and F3/Shift+F3 use asynchronous literal search, with case/whole-word options
and up to 2000 results. Ctrl+Shift+F includes paged tabs. Results select the real
source and use the warning text colour. Paged global results open the source view
instead of duplicating it in a full-text preview. Selection-only search and Replace
all are unavailable in this profile; replacing one selected result is supported.
Search uses physical UTF-8 bytes: use extended CRLF/CR escapes when needed.

One insertion is limited to 1 MiB after newline normalization; copy/cut exports at
most 4 MiB per selection. A larger cross-page selection still supports deletion or
bounded replacement. Cut copies before proposing deletion. These operation budgets
do not limit the document to 4 MiB. Pending input cancelled by a revision/refusal
is retained for manual copying and acknowledgement; it is not replayed automatically.

## Save the entire document {#save}

Ctrl+S streams the complete native document, including unseen pages. Save As
preserves the original and requires a new destination; it refuses existing files,
other open targets, read-only publication and disk conflicts. BOM and untouched
CRLF/CR/LF bytes are preserved. Inserted newlines follow the first convention found
in the first 64 KiB (LF if none). Encoding conversion remains an ordinary-editor
command. Save a copy explicitly before discarding important work.

## Current limits {#limits}

The shared syntax catalogue, including plugins, Verse, HLSL/GLSL and log levels,
uses complete lexical context up to 32 MiB. Above that budget syntax is announced
as unavailable and text is plain; a page never starts with invented lexer state.
Folding, minimap, multi-cursor, snippets/completion, soft wrapping and paged LSP are
not provided in this profile. Whole-text comparisons, history and plugin transforms
require ordinary documents. Their existing behaviour remains available there.

Paged tabs are local native documents; the standalone SDK/MCP session currently
exposes ordinary documents only. Durable crash recovery is unavailable. Unsaved
paged edits and cancelled input are volatile; save useful work explicitly. The
missing common Core session/recovery and CyUI streaming-lexer contracts are
documented in the source developer guide. See [all limits](doc:limits#limits).
