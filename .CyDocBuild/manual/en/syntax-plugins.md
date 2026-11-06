---
id: syntax-plugins
title: Add a syntax
description: Contribute lexical rules or a native token provider.
section: Using Reader
order: 140
parent: plugins
revision: 2
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
