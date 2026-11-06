---
id: mcp
title: Connect an MCP client
description: Copy the read or editing configuration for this running Reader.
section: Using Reader
order: 160
parent: plugins
revision: 2
---

## Connection {#connection}

Reader's optional companion uses the real CyToolsCore SDK. A compatible private Python/SDK installation is separate from the public editor package. The MCP panel reports status and offers start/retry and copy-configuration actions. Copy read access for inspection; select editing deliberately for mutations or plugin calls. Each process has its own private connection context; keep endpoint/identity files private.

## Operations {#operations}

The official adapter exposes `op_<operationId>`. Its arguments wrap operation input in `parameters`, for example `{"parameters":{}}` for `op_documents_list`. Poll `cy_get_job` for Completed/Failed/Cancelled rather than treating submission as success.

Read a document's issued ID and current expectedRevision before replacing, formatting, saving or closing it. Saving is explicit; closing dirty text needs `discard:true`. Opening supplied files requires filesystem access; editing and plugin invocation require their respective permissions. [Public contracts](doc:api-links#contract) document every input/output schema. Native plugin callbacks cannot be forcibly cancelled or sandboxed by the SDK.
