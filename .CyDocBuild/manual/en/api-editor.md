---
id: api-editor
title: Editor
description: Public operations for Editor.
section: API and extensions
parent: api-links
order: 108
revision: 2
---

Public operations for Editor.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`editor_read_selection`](doc:operation-editor-read-selection-editor-read-selection) — Read the actual selected text of an ordinary GUI document, with zero-based UTF-16 coordinates and current native revision.
- [`editor_replace_selection`](doc:operation-editor-replace-selection-editor-replace-selection) — Replace the actual ordinary-editor selection in one Undo transaction at expectedRevision.
- [`editor_undo`](doc:operation-editor-undo-editor-undo) — Use the real GUI undo history at expectedRevision.
- [`editor_redo`](doc:operation-editor-redo-editor-redo) — Use the real GUI redo history at expectedRevision.
- [`editor_goto`](doc:operation-editor-goto-editor-goto) — Move the ordinary GUI caret using zero-based UTF-16 line/character coordinates at expectedRevision.
