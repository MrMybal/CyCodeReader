---
id: editing
title: Edit and save
description: Work with live buffers, Undo and external changes.
section: Using Reader
order: 30
parent: overview
revision: 2
---

## Documents {#documents}

Each tab has a native document and revision. Editing, multiple cursors, selections, indentation, bracket matching and folding operate on its live text. Undo/Redo restores supported editor changes; opening a comparison or applying a filter does not edit a document.

Save checks the original disk baseline and uses guarded atomic replacement. An externally changed file causes a conflict instead of silently replacing newer bytes. Review it before reload or Save As. Reload can abandon unsaved text and asks accordingly. Save As checks existing files and other open targets. Read-only documents refuse edits.

## Encoding {#encoding}

UTF-8/BOM and UTF-16 LE/BE BOM are detected. Windows-1252 is explicit. [Encoding controls](doc:encoding#convert) change pending format in memory; saving is separate. Files are limited to 4 MiB.

## Layout {#layout}

Use View for wrapping, whitespace, minimap, text size and layout reset. Detach a tab or Move to new window for a true native window on another monitor. [Preferences](doc:settings#appearance) controls themes and language. Reader does not restore a complete session or crash recovery history.
