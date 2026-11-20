---
id: overview
title: CyCodeReader
description: Choisissez une tâche dans l’éditeur de code et de texte.
section: Utiliser Reader
order: 0
sourceRevision: 5
---

## Commencer {#getting-started}

CyCodeReader modifie des fichiers de code et de texte sans ouvrir de projet d’IDE. Commencez par [ouvrir des fichiers](doc:quickstart#first-work), la [visite illustrée](doc:tour#interface), l’[édition](doc:editing#documents) ou la [recherche](doc:search#find).

## Plateformes {#platforms}

Les paquets Windows x64 comprennent une édition portable et un installateur par utilisateur. Un pilote OpenGL 3.3 est nécessaire. L’éditeur, le chat natif et le [compagnon MCP](doc:mcp#connection) n’exigent pas Python. Configurez un fournisseur compatible pour l’assistant. Les compilations source Linux/macOS sont prévues ; aucun paquet natif n’est fourni ici.

La 0.1.9 utilise Free comme profil MCP local par défaut : connexion par adresse sans jeton. Les choix explicites sont conservés. Voir les [profils d’accès MCP](doc:mcp#access-profiles) ; la permission d’édition du chat reste indépendante.

## Licence {#licensing}

CyCodeReader est sous GPL-3.0-only. Ses dépendances conservent leurs licences ; consultez les [licences](doc:licences#licence).


La 0.1.10 ajoute la coloration Zig/ZON et les choix manuels de syntaxe mémorisés
pour les fichiers enregistrés. Voir les [modes](doc:syntax-plugins#zig-and-zon).
La coloration n’inclut ni ZLS, ni compilation, ni débogage, ni service sémantique.
