---
id: api-plugins
title: Extensions
description: Public operations for Extensions.
section: API and extensions
parent: api-links
order: 103
revision: 2
---

Public operations for Extensions.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`plugins_list`](doc:operation-plugins-list-plugins-list) — Inspect loaded native/data plugins, commands, options, language definitions and load errors.
- [`plugins_configure`](doc:operation-plugins-configure-plugins-configure) — Validate declared plugin option types and update this instance.
- [`plugins_invoke`](doc:operation-plugins-invoke-plugins-invoke) — Execute a trusted plugin command against an open guarded buffer.
