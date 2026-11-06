---
id: api-plugins
title: Extensions
description: Opérations publiques pour Extensions.
section: API et extensions
parent: api-links
order: 103
sourceRevision: 2
---

Opérations publiques pour Extensions.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`plugins_list`](doc:operation-plugins-list-plugins-list) — Consulter les plugins, commandes, options, syntaxes et erreurs de chargement.
- [`plugins_configure`](doc:operation-plugins-configure-plugins-configure) — Valider les types des options déclarées et modifier les préférences du plugin.
- [`plugins_invoke`](doc:operation-plugins-invoke-plugins-invoke) — Exécuter une commande de plugin de confiance sur un document avec contrôle de révision.
