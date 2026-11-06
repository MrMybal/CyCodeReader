---
id: languages
title: Coloration syntaxique
description: Choisissez une syntaxe intégrée ou fournie par plugin.
section: Utiliser Reader
order: 70
parent: overview
sourceRevision: 3
---

## Modes intégrés {#syntax}

Le catalogue CyUI partagé fournit 102 modes dont C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, Python, Lua, JSON, Markdown, HLSL, GLSL, Verse et les journaux. Le menu Syntaxe les regroupe alphabétiquement et filtre par nom ou extension. Automatique utilise les associations de fichiers ; un choix explicite change l’affichage sans modifier le texte. Les journaux distinguent les lignes Error et Warning.

Rust, Java, JavaScript/JSX, TypeScript/TSX, HTML et CSS utilisent aussi l’analyse incrémentale Tree-sitter, avec les régions script/style HTML. Les autres modes utilisent les adaptateurs du catalogue et lexicaux disponibles. Les [plugins de syntaxe](doc:syntax-plugins#definitions) peuvent ajouter des associations ou fournisseurs de jetons.

## Portée {#services}

La coloration n’installe pas de compilateur et ne fournit ni complétion de serveur de langage, ni renommage, débogueur ou sémantique de projet. Ces services d’IDE appartiennent à CyCode. Un moteur de syntaxe fonctionne sans installer Rust, Java ou Node.js.

## Verse {#verse}

Les fichiers terminés par `.verse`, y compris les fichiers générés `.digest.verse`,
sélectionnent automatiquement Verse. La coloration distingue les mots-clés,
types primitifs, attributs `@editable`, spécificateurs `<public>` / `<suspends>`,
noms d’appels, nombres, chaînes et expressions interpolées. Les commentaires
`#`, imbriqués `<# ... #>` et indentés `<#>` conservent leur état entre les lignes.

```verse
score_counter := class:
    var Score:int = 0
    Add(Amount:int)<transacts>:void =
        set Score += Amount
```

Ce mode de coloration ne démarre pas de serveur de langage Verse et ne compile
pas les projets UEFN. La compilation Verse et la liaison avec Fortnite en cours
d’exécution ne sont pas intégrées. Consultez le
[Book of Verse](https://verselang.github.io/book/00_overview/) pour le langage.
