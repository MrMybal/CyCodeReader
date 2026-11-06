---
id: version-control
title: Comparer fichiers et révisions Git
description: Examinez des instantanés en lecture seule des onglets et du dépôt.
section: Utiliser Reader
order: 110
parent: overview
sourceRevision: 2
---

## Comparer {#providers}

Comparer choisit deux onglets en mémoire, un fichier externe sur disque ou la dernière version enregistrée. Le texte non enregistré est inclus. Choisissez la disposition côte à côte ou unifiée, la recherche et le changement précédent/suivant. Les deux côtés partagent le défilement. Sources et fichiers disque restent inchangés. Actualisez les instantanés périmés après édition.

## Comparaisons Git {#git}

Installez Git dans PATH et gardez actif le plugin fourni de comparaison Git. Son menu détecte le dépôt local du fichier actif. Comparez le texte avec HEAD ou l’index, ou examinez les changements indexés de HEAD à l’index. Le menu contextuel du code ou des lignes ouvre l’historique du fichier ou de la sélection, suit les renommages et compare avec un parent, le texte courant ou une autre révision.

Ces opérations sont locales et en lecture seule. Reader ne prépare pas de commit, ne valide pas et ne lance ni push ni pull. Les fournisseurs utilisent le [contrat de comparaison](doc:comparison-api#contract) ; Git absent, fichiers non suivis et contenus non pris en charge sont signalés.
