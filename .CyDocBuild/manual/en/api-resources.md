---
id: api-resources
title: Resources
description: Public operations for Resources.
section: API and extensions
parent: api-links
order: 106
revision: 2
---

Public operations for Resources.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`resources_read`](doc:operation-resources-read-resources-read) — Read a registered document snapshot or private result chunk by its issued URI; no arbitrary filesystem or URL access.
- [`resources_grep`](doc:operation-resources-grep-resources-grep) — Search registered text resources with native streaming literal grep, using the issued cursor for continuation.
