---
id: api-links
title: Public operation reference
description: Use issued IDs and revisions with the official SDK.
section: Using Reader
order: 170
parent: plugins
revision: 2
---

## Request contract {#contract}

The service subpages document all 13 operations declared by Reader. Signatures and JSON Schemas describe operation input/output, not C++ functions. The official MCP tool arguments use `{"parameters":{...}}`; choose read-only `op_documents_list` with `{"parameters":{}}` to obtain open-document identities.

Use an issued document `id` and the exact current `expectedRevision` for changes. Reader shares the live GUI buffers; a source edit can stale a queued request. A successful native mutation does not imply a disk save. Review the result and save explicitly. Operation IDs and text offsets are technical values, not temporary job-file references.

## Ownership {#transactions}

```diagram
{"type":"flow","title":"Save a document","nodes":[{"id":"read","label":"Read ID and revision","x":0,"y":0},{"id":"edit","label":"Change live text","x":230,"y":0},{"id":"save","label":"Explicit save","x":460,"y":0}],"edges":[{"from":"read","to":"edit"},{"from":"edit","to":"save"}]}
```

The official SDK manages jobs and permissions. Native ReaderCore owns document validation and disk baselines. See [MCP](doc:mcp#operations) and the distinct [plugin ABI](doc:plugin-api#contract).
