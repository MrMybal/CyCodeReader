---
id: mcp
title: Connect an MCP client
description: Copy the read or editing configuration for this running Reader.
section: Using Reader
order: 160
parent: plugins
revision: 3
---

## Connection {#connection}

Reader links CyMCPSDK natively, including CyMCP::ChatUI and the console companion. Open CyMCPSDK / MCP to copy read or editing configurations for the actual running instance. Chat supports streaming, cancellation, provider/model discovery, explicit attachments and private history restoration. Configure a tool-capable provider and reopen a connection after changing edit permissions. API keys come from named environment variables.

## Operations {#operations}

The companion supports `list`, `describe NAME`, `invoke NAME --json JSON`, `jsonl`, `serve` and `mcp`. Use `--connect DESCRIPTOR --access read|edit` for the open instance, or `--file FILE --data-dir DIRECTORY` for a headless session. The original raw `--serve` and Python `--bridge-config` forwarding entry remain available.

Compact discovery exposes `cy_tools_search`, `cy_tools_describe`, `cy_tools_call` and resource readers. Call with `{"name":"documents_list","arguments":{}}`. `--full-tools` exposes direct operation arguments without the old parameters wrapper. The reference covers all 30 GUI operations; 22 also work headlessly. Read identity cannot mutate/open paths. Plugins additionally need allowPlugins:true. Saving is explicit and revision/read-only/disk guards remain authoritative. Paged documents stay GUI-only.

## Results and settings {#results}

Use `jobs_list/get/cancel/forget` for private durable receipts; interrupted work becomes interrupted-unknown after restart and is never replayed. Forgetting requires explicit forget:true,confirm:true. Large temporary results use 32 KiB chunks, memory/disk budgets and TTL; durable receipts outlive these handles. Live changes invalidate document snapshots. Compact discovery, resources, notifications, metrics and API context windows are separately configurable. This host provides request/response HTTP without SSE; direct stdio MCP supports SDK change subscriptions. Other clients use TTL/refresh.

Optional configured remote MCP tools are explicitly allowlisted. Connect them from the panel, then open a new chat connection; remote.ID.NAME requires edit access. No engine/model weights are downloaded. Old SQLite jobs have a read-only importer that preserves original files and never replays actions. See Docs/MCP.md in the source repository for exact switches, source revisions, configuration, recovery and limits.
