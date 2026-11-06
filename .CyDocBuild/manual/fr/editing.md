---
id: editing
title: Modifier et enregistrer
description: Travaillez avec les documents en mémoire, l’annulation et les changements externes.
section: Utiliser Reader
order: 30
parent: overview
sourceRevision: 2
---

## Documents {#documents}

Chaque onglet possède un document natif et une révision. L’édition, les curseurs multiples, sélections, indentations, correspondances de délimiteurs et replis agissent sur son texte en mémoire. Annuler/Rétablir restaure les modifications prises en charge ; ouvrir une comparaison ou appliquer un filtre ne modifie pas le document.

L’enregistrement contrôle l’état initial du disque et utilise un remplacement atomique protégé. Un fichier modifié ailleurs provoque un conflit au lieu de remplacer silencieusement les nouveaux octets. Examinez-le avant rechargement ou Enregistrer sous. Recharger peut abandonner du texte non enregistré et demande un choix adapté. Enregistrer sous contrôle les fichiers existants et les autres destinations ouvertes. Un document en lecture seule refuse les modifications.

## Encodage {#encoding}

UTF-8/BOM et UTF-16 LE/BE BOM sont détectés. Windows-1252 reste explicite. Les [commandes d’encodage](doc:encoding#convert) modifient le format en attente ; enregistrer reste séparé. Les fichiers sont limités à 4 Mio.

## Disposition {#layout}

Affichage règle les retours visuels, espaces, minicarte, taille du texte et réinitialisation de disposition. Détachez un onglet ou choisissez une nouvelle fenêtre pour obtenir une vraie fenêtre native sur un autre écran. Les [préférences](doc:settings#appearance) règlent thèmes et langue. Reader ne restaure pas une session complète ni un historique de récupération après incident.
