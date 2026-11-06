---
id: api-documents
title: Documents
description: Public operations for Documents.
section: API and extensions
parent: api-links
order: 100
revision: 2
---

Public operations for Documents.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`documents_list`](doc:operation-documents-list-documents-list) — List documents currently open in this Reader instance.
- [`documents_read`](doc:operation-documents-read-documents-read) — Read an open live document by issued ID.
- [`documents_open`](doc:operation-documents-open-documents-open) — Open a user file; auto detects UTF-8/BOM and UTF-16 BOM, or use an explicit encoding (Windows-1252 is never guessed).
- [`documents_new`](doc:operation-documents-new-documents-new) — Create an unsaved native document.
- [`documents_replace`](doc:operation-documents-replace-documents-replace) — Replace an open buffer at expectedRevision; does not save.
- [`documents_save`](doc:operation-documents-save-documents-save) — Save an already-named document at expectedRevision using its native disk baseline and guarded atomic replacement.
- [`documents_format`](doc:operation-documents-format-documents-format) — Change the pending encoding/newline convention at expectedRevision.
- [`documents_close`](doc:operation-documents-close-documents-close) — Close an open document at expectedRevision.
