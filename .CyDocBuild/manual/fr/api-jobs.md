---
id: api-jobs
title: Tâches durables
description: Opérations publiques pour Tâches durables.
section: API et extensions
parent: api-links
order: 105
sourceRevision: 2
---

Opérations publiques pour Tâches durables.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`jobs_list`](doc:operation-jobs-list-jobs-list) — Lister les reçus durables appartenant à cette identité, par pages.
- [`jobs_get`](doc:operation-jobs-get-jobs-get) — Récupérer le résultat durable d’une opération sans la rejouer.
- [`jobs_cancel`](doc:operation-jobs-cancel-jobs-cancel) — Demander l’annulation coopérative d’une opération appartenant à cette identité.
- [`jobs_forget`](doc:operation-jobs-forget-jobs-forget) — Supprimer un reçu terminé appartenant à cette identité avec confirmation explicite.
