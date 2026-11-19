---
id: mcp
title: Connecter un client MCP
description: Copiez la configuration de lecture ou d’édition de cette instance.
section: Utiliser Reader
order: 160
parent: plugins
sourceRevision: 3
---

## Connexion {#connection}

Reader intègre désormais CyMCPSDK 0.7.1 en C++ ; aucun service Python ni wheel à installer. Ouvrez **CyMCPSDK / MCP** pour copier la configuration lecture ou édition de l’instance ouverte. Le chat utilise CyMCP::ChatUI avec streaming, annulation, pièces jointes, détection des fournisseurs, modèles et historique privé restaurable. Configurez un fournisseur compatible avec les appels d’outils. L’édition reste désactivée par défaut ; ouvrez une nouvelle connexion pour appliquer ses permissions. Les clés API proviennent d’une variable d’environnement nommée.

## Opérations {#operations}

Le compagnon `cycodereader_cli` accepte `list`, `describe NOM`, `invoke NOM --json JSON`, `jsonl`, `serve` et `mcp`. `--connect FICHIER --access read|edit` cible l’instance réelle. `--file FICHIER --data-dir DOSSIER` ouvre une session sans interface. L’ancienne option native `--serve` et le lanceur Python `--bridge-config` restent compatibles.

La découverte compacte expose `cy_tools_search`, `cy_tools_describe`, `cy_tools_call` et les lecteurs de ressources. Appelez `cy_tools_call` avec `{"name":"documents_list","arguments":{}}` ; `--full-tools` expose directement les opérations sans enveloppe `parameters`. Les 30 contrats sont décrits dans la référence ; 8 nécessitent l’interface (22 en mode autonome). L’identité lecture ne peut ni modifier ni ouvrir des chemins arbitraires. Les plugins nécessitent aussi `allowPlugins:true`. L’enregistrement reste explicite, avec contrôles de révision, lecture seule et conflit disque. Les documents paginés restent accessibles uniquement dans l’interface.

## Résultats et réglages {#results}

`jobs_list/get/cancel/forget` donnent accès aux reçus durables privés. Une opération interrompue devient `interrupted-unknown` au redémarrage, sans rejeu. L’oubli nécessite `forget:true,confirm:true`. Les gros résultats utilisent des fragments de 32 Kio, des quotas mémoire/disque et un TTL réglables ; l’expiration ne relance jamais l’action. Les reçus durables restent disponibles après expiration. Les modifications invalident les instantanés de texte. La découverte compacte, les ressources, notifications, mesures facultatives et fenêtre de contexte sont réglables séparément. HTTP est requête/réponse sans SSE dans cet hôte ; les abonnements pris en charge sont disponibles en MCP stdio direct, les autres clients utilisent TTL/actualisation.

Les serveurs MCP distants sont facultatifs, configurés explicitement et limités par `allowTools`. Connectez-les depuis le panneau puis ouvrez une nouvelle connexion au chat. Les outils `remote.ID.NOM` nécessitent l’édition. Aucun moteur ni poids de modèle n’est téléchargé. Les anciens jobs peuvent être importés en lecture seule par `scripts/import_legacy_jobs.py` ; les données d’origine et documents restent conservés. Les détails de configuration, transport, migration et limites sont dans `Docs/MCP.md` du dépôt.
