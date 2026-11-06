---
id: comparison-api
title: Comparison provider API
description: Return read-only snapshots through declared service commands.
section: Using Reader
order: 150
parent: plugins
revision: 2
---

## Contract {#contract}

Declare `service` commands and `fileComparisons` metadata with a menu, detect command and actions. Detect receives the active absolute `path` and options; return `available` and per-command `enabled` booleans. Optional message/root/branch and tracked/conflicted/detached fields describe context. Maximums are 16 actions and 8 active providers.

An action receives path, normalized UTF-8 text, encoding, native revision and options. Return the comparison below without top-level `text` so it does not request a document edit. The host checks path/revision and displays a bounded read-only snapshot.

```json
{"comparison":{"path":"/absolute/example.cpp","before":"old\n","after":"new\n","beforeLabel":"Baseline","afterLabel":"Buffer"}}
```

The native [ABI](doc:plugin-api#contract) remains UTF-8 JSON; callbacks do not receive editor/ImGui objects. Existing read-only views can be refreshed when the source changes. A provider does not automatically acquire Git write permissions or implement an IDE VCS backend.
