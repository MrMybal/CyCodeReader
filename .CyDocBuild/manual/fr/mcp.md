---
id: mcp
title: Connecter un client MCP
description: Copiez la configuration de lecture ou d’édition de cette instance.
section: Utiliser Reader
order: 160
parent: plugins
sourceRevision: 2
---

## Connexion {#connection}

Le compagnon facultatif de Reader utilise le vrai SDK CyToolsCore. Son installation Python/SDK privée compatible reste séparée du paquet public de l’éditeur. Le panneau MCP affiche l’état et propose démarrage, nouvelle tentative et copie de configuration. Copiez l’accès lecture pour consulter ; choisissez explicitement l’édition pour modifier ou appeler un plugin. Chaque processus a son contexte privé ; conservez les fichiers d’identité et de connexion confidentiels.

## Opérations {#operations}

L’adaptateur officiel expose `op_<operationId>`. Ses arguments placent l’entrée de l’opération dans `parameters`, par exemple `{"parameters":{}}` pour `op_documents_list`. Suivez `cy_get_job` jusqu’à Completed/Failed/Cancelled au lieu d’assimiler l’envoi à une réussite.

Lisez l’identifiant émis et la révision courante avant de remplacer, formater, enregistrer ou fermer un document. Enregistrer est explicite ; fermer un texte modifié exige `discard:true`. Ouvrir un fichier fourni exige l’accès au système de fichiers ; modifier et invoquer un plugin exigent leurs autorisations respectives. Les [contrats publics](doc:api-links#contract) décrivent chaque schéma d’entrée et de sortie. Le SDK ne peut ni isoler ni interrompre de force un callback natif.
