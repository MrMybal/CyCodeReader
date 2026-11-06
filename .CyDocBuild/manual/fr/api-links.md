---
id: api-links
title: Référence des opérations publiques
description: Utilisez identifiants émis et révisions avec le SDK officiel.
section: Utiliser Reader
order: 170
parent: plugins
sourceRevision: 2
---

## Contrat des requêtes {#contract}

Les sous-pages décrivent les 13 opérations déclarées par Reader. Signatures et schémas JSON représentent les entrées et sorties d’opération, pas des fonctions C++. Les arguments MCP officiels utilisent `{"parameters":{...}}` ; appelez `op_documents_list` en lecture seule avec `{"parameters":{}}` pour obtenir les identités des documents ouverts.

Utilisez un `id` émis et la valeur exacte de `expectedRevision` pour modifier. Reader partage les textes de l’interface ; une édition peut périmer une requête en attente. Une modification native réussie n’implique pas d’enregistrement sur disque. Examinez le résultat et enregistrez explicitement. Identifiants d’opération et positions de texte sont des valeurs techniques, pas des références de fichiers temporaires de tâches.

## Responsabilités {#transactions}

```diagram
{"type":"flow","title":"Enregistrer un document","nodes":[{"id":"read","label":"Lire identité et révision","x":0,"y":0},{"id":"edit","label":"Modifier en mémoire","x":230,"y":0},{"id":"save","label":"Enregistrer explicitement","x":460,"y":0}],"edges":[{"from":"read","to":"edit"},{"from":"edit","to":"save"}]}
```

Le SDK officiel gère tâches et autorisations. ReaderCore natif valide les documents et leurs états de disque. Consultez [MCP](doc:mcp#operations) et l’[ABI de plugin](doc:plugin-api#contract), distincte.
