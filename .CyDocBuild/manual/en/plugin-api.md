---
id: plugin-api
title: Native plugin API
description: Include the public ABI and exchange UTF-8 JSON.
section: Using Reader
order: 130
parent: plugins
revision: 2
---

## Entry point {#contract}

Include `PluginABI.h` from the corresponding source distribution's `src` include directory. Build a platform-compatible shared library exporting `cycodereader_plugin_v1`. Its type is `const CyCodeReaderPluginV1* (*)(void)`; it returns a stable plugin-owned descriptor with ABI 1, its structure size and an execute callback. The manifest's `library` is a local stem. Current loader conventions use `.dll` on Windows and `.so` on Linux/macOS.

```cpp
#include "PluginABI.h"
const char* execute(const char* command, const char* input_json);
extern "C" const CyCodeReaderPluginV1* cycodereader_plugin_v1();
```

Export the entry with the platform's visibility mechanism. `command` is the declared command ID; `input_json` is the UTF-8 request including the relevant text/options. `execute` returns UTF-8 JSON owned by the plugin until its next call. Calls are serial. Do not throw across C, pass STL/ImGui objects or access GUI functions. Return a `text` member only when intending a buffer replacement; the host validates size, revision and mutability before applying it.

## Manifests and options {#manifest}

A manifest declares `abi`, unique `id`, `name`, `version` and optional commands/languages/options. Options have boolean, signed 32-bit integer or bounded string types and a default. Native libraries must stay within the plugin directory. Duplicate IDs and incompatible ABI fail. Each manifest is at most 256 KiB; plugin/contribution counts are bounded at 64.

```json
{"abi":1,"id":"sample.rules","name":"Sample rules","version":"1.0.0","commands":[{"id":"trim","title":"Trim trailing whitespace","operation":"trimTrailing"}],"options":[{"id":"enabled","type":"boolean","default":true}]}
```

This data-only example needs no native entry point. A native manifest additionally names `library` and its declared commands. [Syntax](doc:syntax-plugins#definitions) and [comparison](doc:comparison-api#contract) have specialised JSON contracts.
