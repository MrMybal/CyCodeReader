---
id: languages
title: Langages et coloration
description: Langages et coloration — manuel local de l’application
section: Guides
order: 40
sourceRevision: 1
---

## Coloration {#syntax}

Les syntaxes proviennent du catalogue partagé CyUI. Choisissez Automatic ou un langage précis. C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, HLSL, GLSL et les journaux font partie des modes partagés ; le catalogue installé reste la référence pour la liste complète. Les extensions peuvent enregistrer des syntaxes via les contrats documentés.

## Services de langage {#services}

Reader est un éditeur de texte/code. Il ne lance pas les services IDE, serveurs de langage ou débogueurs de CyCode. Une syntaxe Rust ou HLSL ne constitue pas une analyse sémantique ni une compilation de shader.

```hlsl
float4 main(float4 position : SV_Position) : SV_Target
{
    return float4(0.2, 0.6, 1.0, 1.0);
}
```

Exemple de coloration, pas une preuve de compilation sur cette machine.
