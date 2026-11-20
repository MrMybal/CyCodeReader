---
id: syntax-plugins
title: Add a syntax
description: Contribute lexical rules or a native token provider.
section: Using Reader
order: 140
parent: plugins
revision: 3
---

## Definitions {#definitions}

A language is registered as `<plugin-id>/<language-id>` in the shared CyUI catalogue. Declare extensions, keywords, types, identifiers, case sensitivity and line/block comment delimiters. `lexer:"rules"` is the default; `lexer:"inherit", base:"hlsl"` selects an existing catalogue lexer with optional `wordLists`. This does not import Notepad++ UDL XML.

```json
{"id":"config","name":"Configuration","extensions":[".mycfg"],"lexer":"rules","lineComment":"#","keywords":["section","option"],"types":["string","number"]}
```

## Native tokens {#tokens}

Declare a command with `kind:"syntax"` and name it in the language's `providerCommand`. The callback receives `text` and `options`, and returns ordered nonoverlapping UTF-8 byte ranges. Ranges must stay within the snapshot and not split characters.

```json
{"tokens":[{"start":0,"length":6,"role":"keyword"}]}
```

Roles include text, keyword, type, number, string, comment, preprocessor, function, added, removed, heading, warning and error. Gaps stay neutral. On provider failure the tab remains editable and colouring is disabled; select the syntax again to retry after correcting it. This does not add language-server services or load arbitrary Tree-sitter libraries.


## Zig and ZON {#zig-and-zon}

Open `.zig`, `build.zig`, `.zon` or `build.zig.zon` in Automatic mode. Reader uses
the shared CyUI associations, including plugin priority. Choose **Syntax → Zig**
or **ZON** for a manual override, or **Plain text** to disable coloring. Automatic
and manual choices are distinct; the status bar shows the effective mode.

The choice follows edits and Save As, and is restored for saved files when they
are reopened after restarting Reader. Unavailable plugin IDs remain stored; the
view falls back to plain text and marks the syntax unavailable until that plugin
is present again. Other preferences remain intact. Unsaved-buffer, full-session
and Undo recovery are not added by these syntax preferences.

Zig and ZON reuse CyUI's common lexer. Strings/comments, arbitrary-width integer
types, builtins, Unicode source and ZON literals use the common palette. The
editor, open-document search previews and file/history comparisons share the
selected mode. A color change does not edit text or create an Undo entry. See
[paged text](doc:paged-text#limits) for the 32 MiB contextual syntax budget.
This is offline highlighting, without ZLS, a compiler, debugger or semantics.
