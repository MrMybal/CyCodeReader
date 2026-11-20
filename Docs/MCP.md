# Native chat, CLI and MCP

Reader 0.1.9 links CyMCPSDK 0.7.2 built from source. `cymcp-source.lock.json` pins published Git revision `e562c731a146d599e3404718ea8414bb5b96b66d` and every frozen input hash (UTF-8 source text is normalized to LF). The local sibling CyMCPSDK checkout is preferred when it matches; otherwise, if unavailable, the resolver fetches the exact official GitHub revision. Builds use an ignored digest-named snapshot and preserve modified local checkouts. The application has no Python SDK service or wheel dependency.

Reader uses Core `b902736b0edb9a76365a47778af8a223d50e5b83`, native source API 1. One Core TextSession owns the documents. GUI dispatch runs on the UI thread and uses the actual editor and its Undo history. The full Core project Engine is not instantiated. CyUI is pinned at `6ef01cfcdb197f3a89d64618ce5ef8818c704728`.

## Terminal

The console companion `cycodereader_cli` is installed in Reader’s `app/` subdirectory. On Windows append `.exe`. JSON goes to stdout, diagnostics to stderr; invalid arguments return 2, access/authentication denial 3, other failures 1, success 0.

```powershell
.\app\cycodereader_cli.exe --help
.\app\cycodereader_cli.exe --data-dir .\private-cli list
.\app\cycodereader_cli.exe --data-dir .\private-cli describe documents_read
.\app\cycodereader_cli.exe --data-dir .\private-cli invoke automation_status --json '{}'
.\app\cycodereader_cli.exe --access edit --data-dir .\private-cli --file .\example.cpp jsonl
```

`jsonl` keeps the headless session alive, reading one `{ "method":"invoke", "name":"documents_list", "arguments":{} }` object per input line and returning a result/error object. Use issued document IDs and current revisions. `invoke` creates an ephemeral document session; durable receipts remain in the profile. `serve --descriptor FILE` keeps a loopback service alive (Free by default) until stdin closes. The default CLI profile is `%LOCALAPPDATA%/CyCodeReader/automation/cli` on Windows (`$XDG_DATA_HOME/CyCodeReader/automation/cli` elsewhere); pass a separate `--data-dir` for concurrent sessions. An OS lease prevents two owners or false interruption recovery.

The original `--serve DESCRIPTOR [--plugins DIRECTORY] [FILES...]` raw native TextBridge remains compatible. It is distinct from the SDK HTTP service and retains its original authentication. Old Python `app/tool.py --bridge-config FILE --access read|edit mcp` forwards to the installed native companion using only Python’s standard library. The old `--desktop-service --native-descriptor FILE --service-descriptor FILE --data-dir DIRECTORY` command remains a native SDK adapter over that actual raw TextBridge (it stops when that native descriptor disappears); its catalogue omits Save As because the raw TextBridge has no such operation. The GUI owns its SDK service directly. The compatibility service publishes the new SDK descriptor format. Old descriptors/tokens are not silently upgraded: copy a new configuration from Reader.

## Running GUI and external MCP

Open **CyMCPSDK / MCP** in Reader. The service starts with the application; the panel copies the MCP address and configuration, with separate read/edit credentials for Protected. Its profile selector persists the choice for the next restart. `--no-automation` disables it. `--automation-config FILE`, `--automation-data-dir DIRECTORY` and `--automation-descriptor FILE` select an explicit profile/configuration. Default GUI data is the existing Reader data directory plus `automation/default`; old documents/preferences are preserved.

```json
{"mcpServers":{"CyCodeReader":{"command":"C:/Reader/app/cycodereader_cli.exe","args":["--connect","C:/private/connection.json","--access","read","mcp"]}}}
```

`--connect FILE list|describe|invoke|mcp` addresses the actual open instance; no second document model is created. Local HTTP binds only `127.0.0.1`. Free is the default for both new profiles and existing configurations without an explicit choice. Connect directly to the copied `mcpUrl` address, without a token or an extra operation approval. `--connect URL` also accepts that address; `/v1` is the JSON equivalent. The private descriptor reports `accessProfile`, `requireToken` and `controlOperations`; tokenless profiles contain no credentials.

| `accessProfile` | Token | Registered operations |
| --- | --- | --- |
| `free` (default) | None | All allowed, `Policy::allow_all()` |
| `local_controlled` | None | Host policy; anonymous callers have read access |
| `token_only` | Required | All allowed with either valid credential |
| `protected` | Required | Host policy; distinct read/edit credentials |

Set `"accessProfile":"protected"` in settings.json or `--config FILE` to retain token-based controlled access. `access_profile` is accepted as an alias, and `protected_access` aliases `protected`. Existing explicit choices are honored; unknown, mistyped or conflicting profiles fail startup rather than opening access. Configurations without a choice migrate logically to Free while preserving their files and unrelated options. In Protected and Token only, use `Authorization: Bearer TOKEN`; the descriptor contains private `readToken`/`editToken` values. Never share them. Host/Origin checks, JSON/schema validation and transport bounds apply to every profile. Its `/v1` JSON and `/mcp` MCP endpoints use the same handlers and transport-assigned principal; request JSON cannot grant permissions. SDK HTTP is request/response JSON; this host does not wire SSE/GET subscription streams. Stdio MCP supports SDK change subscriptions; legacy clients use TTL and explicit refresh. The local MCP proxy exposes tools; use the direct HTTP endpoint for the instance’s resource catalogue.

`resources:false` disables document snapshots and the MCP resource catalogue; private result chunk readers remain available so large calls stay recoverable.

Compact discovery defaults to `cy_tools_search`, `cy_tools_describe`, `cy_tools_call`, `cy_resources_read`, `cy_resources_grep`. Example call arguments: `{"name":"documents_list","arguments":{}}`. Pass `--full-tools` for all operation schemas and direct calls such as `documents_read` with `{"id":1}`. The old `op_` prefix is accepted by direct CLI invocation only. There is no `parameters` wrapper in the new MCP tools.

## Shared operation coverage and rights

The canonical catalogue is `app/CyTool.json`; build-generated C++ and public API documentation consume it. All entry points enforce the same schemas/policy. There are 30 operations in the GUI and 22 in the headless profile. Document list/read/search, plugins list, receipts/resources/status/metrics are available in both. In controlled profiles, new/open/replace/save/save-as/format/close/transforms require the edit identity; plugin execution additionally requires host `allowPlugins:true` (default false). Read identity cannot mutate or open arbitrary supplied paths. Free and Token only authorize every registered operation, including plugin calls, without a separate policy prompt. GUI-only operations cover view state/preferences, tab activation, selection, caret, Undo and Redo. Editor selection/caret positions are zero-based UTF-16; native search offsets remain zero-based UTF-8 bytes with one-based lines/columns.

Saving is explicit. Stale revision, read-only buffer, dirty-close and disk-baseline guards remain authoritative. Save As refuses an existing path; use the GUI overwrite confirmation. Cancellation checks queued work before execution; a committed mutation is never undone by cancelling its response. In-process native plugins are trusted and cannot be forcibly interrupted. Paged documents remain GUI-only: no duplicate SDK session is fabricated. Native file dialogs, OS associations, updates/installation, comparison/history windows and arbitrary shell commands are intentionally excluded from automation; use their existing user confirmation/UI flows.

## Integrated assistant

The panel uses **CyMCP::ChatUI** and the SDK Conversation/ProviderRegistry, with streaming Markdown, cancel/error states, provider detection, real model lists, attachment chips and private history restoration. Configure a tool-capable `openai-compatible` endpoint (Ollama/LM Studio/llama.cpp services can provide local models) or a supported native agent provider. Reader does not download engines or model weights. API keys are read from a named environment variable, not persisted in settings. Fields edited in the panel apply to that session; put persistent connection values in the profile settings.json or an explicit --automation-config file. **Allow document edits** is an independent chat permission, off by default even when the local MCP profile is Free; reopen a connection after changing permissions/tool discovery. Files are attached only after selecting their explicit path. API context-window reduction is adjustable; full private history remains stored, system/current turn and tool-call/result groups are preserved by the SDK. CLI agents keep native context management.

Optional `remoteServers` is an explicit user allowlist. GUI **Connect configured MCP servers** performs discovery on a worker; open a new chat afterwards to refresh tools. Remote tools appear as `remote.ID.NAME`, require edit permission and use SDK McpClient pagination/cache. This host does not subscribe to remote notifications; reconnect/restart for changes, with SDK TTL for compatible reads. Remote failures propagate to the assistant, without automatic retries. Only configure trusted executable commands/URLs; no default server is contacted.

```json
{
  "accessProfile":"free",
  "compact":true,"resources":true,"notifications":true,"metrics":false,
  "diskResults":true,"allowPlugins":false,"inlineBytes":16384,
  "resultMemoryBytes":67108864,"resultDiskBytes":536870912,
  "resultTtlSeconds":300,"cacheTtlSeconds":30,
  "durableJobBytes":536870912,"maxDurableJobs":1024,
  "chat":{"provider":"openai-compatible","baseUrl":"http://127.0.0.1:11434/v1","model":"your-tool-capable-model","apiKeyEnv":"READER_AI_KEY","contextEnabled":false,"contextKiB":256,"recentTurns":8},
  "remoteServers":[{"id":"other","command":"C:/Other/cli.exe","args":["mcp"],"allowTools":["inspect"]}]
}
```

## Results, recovery and performance

Business calls write private durable receipts before dispatch and persist their authoritative outcome before returning. `jobs_list` is owner-scoped/paginated, `jobs_get` retrieves the complete retained result, `jobs_cancel` is cooperative, `jobs_forget` requires explicit `forget:true,confirm:true` and refuses active work. On restart an unfinished receipt becomes `interrupted-unknown`, never replayed. The durable ledger has a separately adjustable count/byte budget; preflight reserves 64 MiB per active ordinary-document operation. A full ledger refuses new work before execution; inspect/export then forget old receipts explicitly. Disk-full/write failures after an action require reconciliation with the live document; no storage system promises recovery after physical storage failure.

Compact calls move large results to SDK private references, 32 KiB chunks, memory/disk budgets and TTL. Handles belong to the calling identity and are temporary; expiration/unknown handles return an error and never replay the operation. The durable receipt survives SDK TTL and restart. Explicitly read text is exposed as revisioned immutable resources for native grep. Live changes remove registered document snapshots and publish changes so stale content cannot be read as current. No recursive filesystem search/ripgrep is enabled. Metrics are opt-in private metadata JSONL (durations, byte counts, status/cache), excluding messages, arguments, results and secrets.

Core/Platform/Http/MCP are linked in the console; AI/ChatUI additionally in the GUI. Local, examples, tests, benchmarks and standalone ChatApp modules are off. Optimizations are independently configurable as above; `automation_status` reports effective configuration and exact SDK fingerprint.

## Legacy data

The package also contains this standard-library-only helper as `app/import_legacy_jobs.py`; Python is needed only to run that optional migration helper or the compatibility launcher. Stop the old service before importing.

Old job SQLite databases and task artifacts are preserved in place and never opened as a new native profile. `scripts/import_legacy_jobs.py --database OLD_DB --destination NEW_PROFILE --client OLD_CLIENT_ID --owner read|edit` performs a standard-library, read-only import of job records to durable receipts, without importing credentials or replaying jobs. First initialize the destination with `cycodereader_cli --data-dir NEW_PROFILE invoke automation_status`; then stop its service. Review ownership and use a destination with no live service. Imported results retain their original metadata/artifact paths; keep the old private artifact directory to access those files. User documents are never copied into job artifacts. Old authentication identities are intentionally not reused; new service tokens are generated at startup only for profiles requiring them.
