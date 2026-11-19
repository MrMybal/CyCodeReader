---
id: api-ui
title: Interface
description: Public operations for Interface.
section: API and extensions
parent: api-links
order: 107
revision: 2
---

Public operations for Interface.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`ui_state`](doc:operation-ui-state-ui-state) — Inspect actual GUI tabs, active document and appearance without copying all document texts.
- [`ui_activate_document`](doc:operation-ui-activate-document-ui-activate-document) — Focus an already-open document by its issued identity.
- [`ui_preferences`](doc:operation-ui-preferences-ui-preferences) — Change selected view options on the GUI thread; no document save or arbitrary command execution.
