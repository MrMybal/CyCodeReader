---
id: limits
title: Capabilities and limits
description: Understand size, search, plugin and project boundaries.
section: Using Reader
order: 190
parent: overview
revision: 2
---

## Scope {#limits}

Reader is a code/text editor, not the complete CyCode IDE. It has no project build system, language servers, semantic refactoring, debugger or recursive folder search. Text files are limited to 4 MiB. Search is literal, with ASCII-only case folding; regex is absent. Crash recovery and complete session/Undo restoration are not supplied. Native plugins are trusted in-process code with no hot unload or sandbox.

## Languages {#translations}

This manual is complete in English and French. API identifiers, literal filenames and code retain their technical spelling. Existing screenshots show sample documents in an English interface; their explanations are translated.

## Reader {#reader}

The whole manual works locally without a web server, CDN or connection. Ctrl+K searches it. Six reader themes are available independently of the app's theme. Images enlarge inside the window and Escape reduces them. Browser storage governs bookmarks, notes and reading positions. External licence/repository links are optional online destinations.
