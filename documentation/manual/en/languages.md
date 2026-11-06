---
id: languages
title: Languages and highlighting
description: Languages and highlighting — local application manual
section: Guides
order: 40
revision: 1
---

## Highlighting {#syntax}

Syntax modes come from the shared CyUI catalogue. Choose Automatic or a specific mode. Shared modes include C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, HLSL, GLSL and logs; the installed catalogue is authoritative for the complete list. Extensions can register syntaxes through the documented contracts.

## Language services {#services}

Reader is a text/code editor. It does not launch CyCode IDE services, language servers or debuggers. Rust or HLSL highlighting does not imply semantic analysis or shader compilation.

```hlsl
float4 main(float4 position : SV_Position) : SV_Target
{
    return float4(0.2, 0.6, 1.0, 1.0);
}
```

Highlighting example, not evidence of compiler execution on this machine.
