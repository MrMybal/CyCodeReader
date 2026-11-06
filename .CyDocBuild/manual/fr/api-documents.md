---
id: api-documents
title: Documents
description: Opérations publiques pour Documents.
section: API et extensions
parent: api-links
order: 100
sourceRevision: 2
---

Opérations publiques pour Documents.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`documents_list`](doc:operation-documents-list-documents-list) — Lister les documents ouverts dans cette instance de Reader.
- [`documents_read`](doc:operation-documents-read-documents-read) — Lire un document en mémoire à partir de son identifiant émis.
- [`documents_open`](doc:operation-documents-open-documents-open) — Ouvrir un fichier utilisateur avec détection UTF-8/BOM ou UTF-16 BOM, ou un encodage explicite.
- [`documents_new`](doc:operation-documents-new-documents-new) — Créer un document natif sans fichier.
- [`documents_replace`](doc:operation-documents-replace-documents-replace) — Remplacer un document à la révision attendue, sans enregistrer sur disque.
- [`documents_save`](doc:operation-documents-save-documents-save) — Enregistrer un document nommé à la révision attendue après contrôle du disque.
- [`documents_format`](doc:operation-documents-format-documents-format) — Modifier l’encodage ou les fins de ligne en attente à la révision attendue.
- [`documents_close`](doc:operation-documents-close-documents-close) — Fermer un document à la révision attendue ; abandonner les changements exige un choix explicite.
