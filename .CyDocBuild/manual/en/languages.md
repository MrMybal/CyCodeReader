---
id: languages
title: Syntax highlighting
description: Choose a built-in or plugin syntax mode.
section: Using Reader
order: 70
parent: overview
revision: 2
---

## Built-in modes {#syntax}

The shared CyUI catalogue provides 101 modes including C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, Python, Lua, JSON, Markdown, HLSL, GLSL and logs. The Syntax menu groups them alphabetically and filters by name/extension. Automatic uses file associations; an explicit selection changes display without editing text. Logs distinguish Error and Warning lines.

Rust, Java, JavaScript/JSX, TypeScript/TSX, HTML and CSS also use incremental Tree-sitter parsing, with HTML script/style regions. Other modes use the catalogue/lexical adapters as available. [Syntax plugins](doc:syntax-plugins#definitions) can add associations or token providers.

## Scope {#services}

Colouring does not install a compiler or provide language-server completion, rename, debugger or project semantics. Those IDE services belong to CyCode. A syntax engine works without installing Rust, Java or Node.js.
