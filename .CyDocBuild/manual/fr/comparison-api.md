---
id: comparison-api
title: API des fournisseurs de comparaison
description: Retournez des instantanés en lecture seule par commandes de service.
section: Utiliser Reader
order: 150
parent: plugins
sourceRevision: 2
---

## Contrat {#contract}

Déclarez des commandes `service` et des métadonnées `fileComparisons` avec menu, commande de détection et actions. La détection reçoit `path` absolu du fichier actif et les options ; retournez `available` et les booléens `enabled` par commande. Les champs facultatifs message/root/branch et tracked/conflicted/detached décrivent le contexte. Les limites sont 16 actions et 8 fournisseurs actifs.

Une action reçoit chemin, texte normalisé UTF-8, encodage, révision native et options. Retournez la comparaison ci-dessous sans `text` de premier niveau pour ne pas demander une modification. L’hôte contrôle chemin et révision et affiche un instantané borné en lecture seule.

```json
{"comparison":{"path":"/absolute/example.cpp","before":"old\n","after":"new\n","beforeLabel":"Baseline","afterLabel":"Buffer"}}
```

L’[ABI native](doc:plugin-api#contract) reste du JSON UTF-8 ; les callbacks ne reçoivent pas d’objets éditeur ou ImGui. Les vues en lecture seule s’actualisent quand la source change. Un fournisseur n’acquiert pas automatiquement des droits d’écriture Git et n’implémente pas un moteur de versions d’IDE.
