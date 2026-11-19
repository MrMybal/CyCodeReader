---
id: api-jobs
title: Durable jobs
description: Public operations for Durable jobs.
section: API and extensions
parent: api-links
order: 105
revision: 2
---

Public operations for Durable jobs.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`jobs_list`](doc:operation-jobs-list-jobs-list) — List only this authenticated identity's durable operation receipts, newest first.
- [`jobs_get`](doc:operation-jobs-get-jobs-get) — Recover the complete durable result/status for an issued job identity, including after transient SDK result expiry.
- [`jobs_cancel`](doc:operation-jobs-cancel-jobs-cancel) — Request cooperative cancellation of this identity's active job.
- [`jobs_forget`](doc:operation-jobs-forget-jobs-forget) — Delete one owned, completed durable receipt only with explicit confirm=true.
