---
id: search
title: Find and replace
description: Choose current-document or all-open-document scope.
section: Using Reader
order: 50
parent: overview
revision: 4
---

## Find in a document {#find}

Ctrl+F opens the separate modeless Find / replace window; Ctrl+H selects replacement. F3/Shift+F3 selects next/previous. Choose case, whole word, selection-only and wrap options. Extended input recognises `\n`, `\r`, `\t` and `\\`. Matching is literal; regex is not implemented. With case off, folding is ASCII-based.

Matches are highlights and minimap/scrollbar marks, not edits. Closing Find keeps marks; Clear search marks removes them. Replace all is one validated Undo transaction in the current document and refuses a truncated match set.

## Search all open documents {#results}

Ctrl+Shift+F searches every open live buffer, including untitled, unsaved and read-only documents. It does not recursively search a project folder. Select a result to preview it, then double-click or Open result to activate the source occurrence. Filters narrow paths/text. Source changes invalidate old positions; refresh the query.

Queries accept up to 4096 bytes and results truncate beyond 2000 matches. A file-name palette is Ctrl+P; actions use Ctrl+Shift+P. [Menu filters](doc:filtering#filters) have separate focus and query state.

The selected source preview highlights every occurrence of the query, using the current case and whole-word options. These highlights follow query changes and also appear in the source editor; a previous document-only selection scope does not restrict global search marks.


[Large UTF-8 files](doc:paged-text#search) use asynchronous search and navigate results in their paged source. Full-text previews, selection-only search and replace-all are unavailable in that profile.
