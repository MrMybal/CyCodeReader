# CyToolsCore SDK and MCP

`scripts/setup_sdk.py` creates runtime/python and verifies the owner's CyToolsCore
0.10.1 wheel hash, then installs it with official MCP SDK 1.30.0. The wheel/runtime
are not distributed by this source repository. GUI editing works without Python;
the MCP panel reports missing/runtime failures and provides a start/retry action.

The GUI's ReaderCore and CLI use exactly the same native operations. GUI tabs hold
shared live Document references; Python forwards requests, without an editor model.
The Core owns schemas, validation, jobs, identities, permissions and task cleanup.
Each Reader process starts one shared SDK HTTP service and can connect multiple
official MCP clients, with distinct read/edit identities and private job visibility.
Native and Core endpoint files stay in data/bridge/<process-id>; never commit them.

Use the GUI's read/edit copy-configuration buttons after the SDK service is ready.
Their argv calls app/tool.py --bridge-config PRIVATE_CORE_DESCRIPTOR --access
read|edit mcp. Tokens stay in the private descriptor, not in the copied arguments.
CYCODE_READER_PYTHON selects a compatible interpreter; default is runtime/python.

Headless development can start `cycodereader_cli --serve NATIVE_DESCRIPTOR
--plugins PLUGIN_DIRECTORY FILE...` and keep its stdin open. Then run:

```text
python app/tool.py --desktop-service --native-descriptor NATIVE_DESCRIPTOR --service-descriptor CORE_DESCRIPTOR --data-dir JOB_DIRECTORY
python app/tool.py --bridge-config CORE_DESCRIPTOR --access read mcp
```

The official SDK exposes `op_<operationId>` from app/CyTool.json. Pass
`{"parameters":{...}}`, then poll cy_get_job for Completed/Failed/Cancelled.
Read-only operations list/read open documents, search live text and inspect plugins.
Mutations require workspace.write and a document ID/current expectedRevision.
Opening an arbitrary supplied file additionally requires filesystem.read; plugin
invocation requires plugins.execute. Unsaved close requires explicit discard=true.
Saving never happens implicitly and checks the native disk baseline.

All paths refer to persistent user source documents. `configure_inputs(...fields={})`
keeps them outside SDK task staging/cleanup. Native plugin code is trusted in-process;
SDK cancellation checks before/after it and cannot forcibly terminate its callback.
The GUI and SDK use bounded logs, requests, document sizes and search results.

Reproduce the official two-client acceptance with:

```sh
runtime/python/Scripts/python.exe -m unittest discover -s tests -p test_mcp.py -v
```

On POSIX use runtime/python/bin/python. CYCODE_READER_TEST_CLI can select the
core-built CLI. The test uses actual native/data plugins, Unicode file paths,
live-buffer edits/search, explicit save, schema validation, denied reader actions,
private-job isolation, stale revisions, dirty close and disk conflicts. No desktop
window is created.
