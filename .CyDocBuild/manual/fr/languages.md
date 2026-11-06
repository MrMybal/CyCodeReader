---
id: languages
title: Coloration syntaxique
description: Choisissez une syntaxe intégrée ou fournie par plugin.
section: Utiliser Reader
order: 70
parent: overview
sourceRevision: 2
---

## Modes intégrés {#syntax}

Le catalogue CyUI partagé fournit 101 modes dont C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, Python, Lua, JSON, Markdown, HLSL, GLSL et les journaux. Le menu Syntaxe les regroupe alphabétiquement et filtre par nom ou extension. Automatique utilise les associations de fichiers ; un choix explicite change l’affichage sans modifier le texte. Les journaux distinguent les lignes Error et Warning.

Rust, Java, JavaScript/JSX, TypeScript/TSX, HTML et CSS utilisent aussi l’analyse incrémentale Tree-sitter, avec les régions script/style HTML. Les autres modes utilisent les adaptateurs du catalogue et lexicaux disponibles. Les [plugins de syntaxe](doc:syntax-plugins#definitions) peuvent ajouter des associations ou fournisseurs de jetons.

## Portée {#services}

La coloration n’installe pas de compilateur et ne fournit ni complétion de serveur de langage, ni renommage, débogueur ou sémantique de projet. Ces services d’IDE appartiennent à CyCode. Un moteur de syntaxe fonctionne sans installer Rust, Java ou Node.js.
