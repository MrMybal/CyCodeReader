---
id: mcp
title: Connect an MCP client
description: Connect by address with Free, or select a controlled or token profile.
section: Using Reader
order: 160
parent: plugins
revision: 4
---

## Connection {#connection}

Reader links CyMCPSDK natively, including CyMCP::ChatUI and the console companion. Open CyMCPSDK / MCP to copy its address or configuration; Free is the default. Controlled token profiles provide separate read/edit credentials. Chat supports streaming, cancellation, provider/model discovery, explicit attachments and private history restoration. Configure a tool-capable provider and reopen a connection after changing edit permissions. API keys come from named environment variables.

## Operations {#operations}

The companion supports `list`, `describe NAME`, `invoke NAME --json JSON`, `jsonl`, `serve` and `mcp`. Use `--connect DESCRIPTOR|URL --access read|edit` for the open instance, or `--file FILE --data-dir DIRECTORY` for a headless session. The original raw `--serve` and Python `--bridge-config` forwarding entry remain available.

Compact discovery exposes `cy_tools_search`, `cy_tools_describe`, `cy_tools_call` and resource readers. Call with `{"name":"documents_list","arguments":{}}`. `--full-tools` exposes direct operation arguments without the old parameters wrapper. The reference covers all 30 GUI operations; 22 also work headlessly. In controlled profiles, read identity cannot mutate/open paths; plugins additionally need allowPlugins:true. Saving is explicit and revision/read-only/disk guards remain authoritative. Paged documents stay GUI-only.

## Results and settings {#results}

Use `jobs_list/get/cancel/forget` for private durable receipts; interrupted work becomes interrupted-unknown after restart and is never replayed. Forgetting requires explicit forget:true,confirm:true. Large temporary results use 32 KiB chunks, memory/disk budgets and TTL; durable receipts outlive these handles. Live changes invalidate document snapshots. Compact discovery, resources, notifications, metrics and API context windows are separately configurable. This host provides request/response HTTP without SSE; direct stdio MCP supports SDK change subscriptions. Other clients use TTL/refresh.

Optional configured remote MCP tools are explicitly allowlisted. Connect them from the panel, then open a new chat connection; remote.ID.NAME requires edit access. No engine/model weights are downloaded. Old SQLite jobs have a read-only importer that preserves original files and never replays actions. See Docs/MCP.md in the source repository for exact switches, source revisions, configuration, recovery and limits.

## Local MCP access profiles {#access-profiles}

Free is the default, including existing configurations without a chosen profile. Copy the MCP address in **CyMCPSDK / MCP** and connect directly, without a token or an additional operation approval. Every registered operation is allowed; schema validation, live revision, read-only and disk-conflict guards still apply. Saving is explicit. The assistant’s **Allow document edits** remains a separate permission, off by default.

The panel’s **MCP access profile** selector saves the choice for the next restart. In settings.json, use `accessProfile`: `free` (address only, all operations), `local_controlled` (address only, host read policy), `token_only` (token, all operations), or `protected` (token, read/edit policy). Explicit existing choices are preserved; `access_profile` and `protected_access` are accepted aliases. Unknown choices fail startup. Protected still requires a valid token. `cycodereader_cli --connect http://127.0.0.1:PORT/mcp invoke documents_list --json '{}'` connects to Free by address alone.
