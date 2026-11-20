---
id: mcp
title: Connecter un client MCP
description: Connectez-vous par adresse avec Free, ou choisissez un profil contrôlé ou avec jeton.
section: Utiliser Reader
order: 160
parent: plugins
sourceRevision: 4
---

## Connexion {#connection}

Reader intègre désormais CyMCPSDK 0.7.2 en C++ ; aucun service Python ni wheel à installer. Ouvrez **CyMCPSDK / MCP** pour copier son adresse ou sa configuration ; Free est le profil par défaut. Le profil Protégé fournit des identités lecture et édition distinctes. Le chat utilise CyMCP::ChatUI avec streaming, annulation, pièces jointes, détection des fournisseurs, modèles et historique privé restaurable. Configurez un fournisseur compatible avec les appels d’outils. L’édition reste désactivée par défaut ; ouvrez une nouvelle connexion pour appliquer ses permissions. Les clés API proviennent d’une variable d’environnement nommée.

## Opérations {#operations}

Le compagnon `cycodereader_cli` accepte `list`, `describe NOM`, `invoke NOM --json JSON`, `jsonl`, `serve` et `mcp`. `--connect FICHIER|URL --access read|edit` cible l’instance réelle. `--file FICHIER --data-dir DOSSIER` ouvre une session sans interface. L’ancienne option native `--serve` et le lanceur Python `--bridge-config` restent compatibles.

La découverte compacte expose `cy_tools_search`, `cy_tools_describe`, `cy_tools_call` et les lecteurs de ressources. Appelez `cy_tools_call` avec `{"name":"documents_list","arguments":{}}` ; `--full-tools` expose directement les opérations sans enveloppe `parameters`. Les 30 contrats sont décrits dans la référence ; 8 nécessitent l’interface (22 en mode autonome). Dans les profils contrôlés, l’identité lecture ne peut ni modifier ni ouvrir des chemins arbitraires ; les plugins nécessitent aussi `allowPlugins:true`. L’enregistrement reste explicite, avec contrôles de révision, lecture seule et conflit disque. Les documents paginés restent accessibles uniquement dans l’interface.

## Résultats et réglages {#results}

`jobs_list/get/cancel/forget` donnent accès aux reçus durables privés. Une opération interrompue devient `interrupted-unknown` au redémarrage, sans rejeu. L’oubli nécessite `forget:true,confirm:true`. Les gros résultats utilisent des fragments de 32 Kio, des quotas mémoire/disque et un TTL réglables ; l’expiration ne relance jamais l’action. Les reçus durables restent disponibles après expiration. Les modifications invalident les instantanés de texte. La découverte compacte, les ressources, notifications, mesures facultatives et fenêtre de contexte sont réglables séparément. HTTP est requête/réponse sans SSE dans cet hôte ; les abonnements pris en charge sont disponibles en MCP stdio direct, les autres clients utilisent TTL/actualisation.

Les serveurs MCP distants sont facultatifs, configurés explicitement et limités par `allowTools`. Connectez-les depuis le panneau puis ouvrez une nouvelle connexion au chat. Les outils `remote.ID.NOM` nécessitent l’édition. Aucun moteur ni poids de modèle n’est téléchargé. Les anciens jobs peuvent être importés en lecture seule par `scripts/import_legacy_jobs.py` ; les données d’origine et documents restent conservés. Les détails de configuration, transport, migration et limites sont dans `Docs/MCP.md` du dépôt.

## Profils d’accès MCP local {#access-profiles}

Free est le profil par défaut, y compris pour les configurations existantes sans choix explicite. Copiez l’adresse MCP dans **CyMCPSDK / MCP** et connectez-vous directement, sans jeton ni confirmation d’opération supplémentaire. Toutes les opérations enregistrées sont autorisées ; la validation des arguments et les contrôles de révision, lecture seule et conflit disque restent actifs. L’enregistrement reste explicite. **Autoriser la modification des documents** demeure une permission indépendante du chat, désactivée par défaut.

Le sélecteur **Profil d’accès MCP** conserve le choix pour le prochain redémarrage. Dans settings.json, utilisez `accessProfile` : `free` (adresse seule, toutes les opérations), `local_controlled` (adresse seule, politique de lecture de l’hôte), `token_only` (jeton, toutes les opérations), ou `protected` (jeton, politique lecture/édition). Les choix explicites existants sont respectés ; `access_profile` et `protected_access` sont acceptés comme alias. Un choix inconnu bloque le démarrage du service. Protégé exige toujours un jeton valide. `cycodereader_cli --connect http://127.0.0.1:PORT/mcp invoke documents_list --json '{}'` se connecte à Free avec l’adresse seule.
