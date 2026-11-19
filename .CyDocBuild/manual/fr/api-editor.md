---
id: api-editor
title: Éditeur
description: Opérations publiques pour Éditeur.
section: API et extensions
parent: api-links
order: 108
sourceRevision: 2
---

Opérations publiques pour Éditeur.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`editor_read_selection`](doc:operation-editor-read-selection-editor-read-selection) — Lire la sélection réelle et ses coordonnées UTF-16.
- [`editor_replace_selection`](doc:operation-editor-replace-selection-editor-replace-selection) — Remplacer la sélection réelle à la révision attendue, sans enregistrer.
- [`editor_undo`](doc:operation-editor-undo-editor-undo) — Annuler une modification dans l’historique du véritable éditeur.
- [`editor_redo`](doc:operation-editor-redo-editor-redo) — Rétablir une modification dans l’historique du véritable éditeur.
- [`editor_goto`](doc:operation-editor-goto-editor-goto) — Déplacer le curseur à une position UTF-16 valide.
