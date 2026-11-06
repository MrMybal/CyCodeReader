---
id: tour
title: Visite illustrée
description: Visite illustrée — manuel local de l’application
section: Guides
order: 15
sourceRevision: 2
---

## Repères de l’interface {#interface}

Le plan suivant explique les zones de l’application ; il s’agit d’un schéma de disposition. L’agrandissement et le zoom restent dans le lecteur.

```image
{"id": "interface-image", "src": "assets/interface-map.svg", "alt": "CyCodeReader interface map / plan explicatif", "controls": "minimal"}
```

La galerie reprend les captures natives de documents exemples de la version 0.1.1, antérieures au menu d’aide ajouté par cette migration. [Méthode et limites des captures](doc:reference-docs-images-readme).

```gallery
{"id": "interface-gallery", "controls": "minimal", "items": [{"type": "image", "src": "assets/interface-map.svg", "alt": "CyCodeReader layout map"}, {"type": "image", "src": "assets/screenshots/editor.png", "alt": "Editor / Éditeur"}, {"type": "image", "src": "assets/screenshots/find-replace.png", "alt": "Find and replace / Chercher et remplacer"}, {"type": "image", "src": "assets/screenshots/search-documents.png", "alt": "Open-document search / Documents ouverts"}, {"type": "image", "src": "assets/screenshots/compare.png", "alt": "Read-only comparison / Comparaison en lecture seule"}, {"type": "image", "src": "assets/screenshots/git-history.png", "alt": "Git history / Historique Git"}]}
```

## Parcours local de l’aide {#help-flow}

```diagram
{"type": "flow", "title": "Offline help / Aide locale", "nodes": [{"id": "app", "label": "CyCodeReader", "x": 10, "y": 10}, {"id": "url", "label": "file:// + stable route", "x": 280, "y": 10}, {"id": "reader", "label": "CyDoc reader", "x": 550, "y": 10}, {"id": "local", "label": "Local data / assets", "x": 550, "y": 150}], "edges": [{"from": "app", "to": "url"}, {"from": "url", "to": "reader"}, {"from": "reader", "to": "local"}]}
```
