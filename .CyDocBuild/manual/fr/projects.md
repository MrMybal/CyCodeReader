---
id: projects
title: Fichiers, dossiers et fenêtres
description: Gardez des ensembles de fichiers indépendants sans projet d’IDE.
section: Utiliser Reader
order: 80
parent: overview
sourceRevision: 2
---

## Ensembles de fichiers {#projects}

Reader gère des documents ouverts plutôt qu’un modèle de solution ou de compilation. Ouvrez directement les fichiers et choisissez un onglet par la liste latérale ou Ctrl+P. Détacher un onglet ou le déplacer vers une nouvelle fenêtre crée une fenêtre native séparée. Un autre processus de l’application garde un ensemble de fichiers indépendant.

## Textes Unreal {#unreal}

Les modes C++/Unreal colorent les sources et jetons de macros. `.uproject`, `.uplugin` et `.sln` restent des textes. Reader ne lance pas UnrealBuildTool, ne génère pas de base de compilation et ne modifie pas les assets Blueprint. Utilisez CyCode pour ces services de projet.

## Exemple {#example}

Ouvrez ensemble `Source/main.cpp`, un shader et une configuration. Cherchez dans leur texte en mémoire avec Ctrl+Shift+F et comparez deux onglets. Aucun compilateur ni import de projet n’est nécessaire.
