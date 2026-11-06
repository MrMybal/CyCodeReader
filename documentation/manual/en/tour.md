---
id: tour
title: Visual tour
description: Visual tour — local application manual
section: Guides
order: 15
revision: 2
---

## Interface landmarks {#interface}

The following layout map explains application regions. It is an explanatory diagram. Enlargement and zoom remain inside the reader.

```image
{"id": "interface-image", "src": "assets/interface-map.svg", "alt": "CyCodeReader interface map / plan explicatif", "controls": "minimal"}
```

The gallery preserves the v0.1.1 native draw-data captures of sample documents. They predate the Help menu added by this migration. [Capture method and limits](doc:reference-docs-images-readme).

```gallery
{"id": "interface-gallery", "controls": "minimal", "items": [{"type": "image", "src": "assets/interface-map.svg", "alt": "CyCodeReader layout map"}, {"type": "image", "src": "assets/screenshots/editor.png", "alt": "Editor / Éditeur"}, {"type": "image", "src": "assets/screenshots/find-replace.png", "alt": "Find and replace / Chercher et remplacer"}, {"type": "image", "src": "assets/screenshots/search-documents.png", "alt": "Open-document search / Documents ouverts"}, {"type": "image", "src": "assets/screenshots/compare.png", "alt": "Read-only comparison / Comparaison en lecture seule"}, {"type": "image", "src": "assets/screenshots/git-history.png", "alt": "Git history / Historique Git"}]}
```

## Local help flow {#help-flow}

```diagram
{"type": "flow", "title": "Offline help / Aide locale", "nodes": [{"id": "app", "label": "CyCodeReader", "x": 10, "y": 10}, {"id": "url", "label": "file:// + stable route", "x": 280, "y": 10}, {"id": "reader", "label": "CyDoc reader", "x": 550, "y": 10}, {"id": "local", "label": "Local data / assets", "x": 550, "y": 150}], "edges": [{"from": "app", "to": "url"}, {"from": "url", "to": "reader"}, {"from": "reader", "to": "local"}]}
```
