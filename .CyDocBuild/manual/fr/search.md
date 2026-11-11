---
id: search
title: Rechercher et remplacer
description: Choisissez le document courant ou tous les documents ouverts.
section: Utiliser Reader
order: 50
parent: overview
sourceRevision: 4
---

## Chercher dans un document {#find}

Ctrl+F ouvre la fenêtre indépendante et non modale Rechercher / remplacer ; Ctrl+H choisit le remplacement. F3/Shift+F3 sélectionne la correspondance suivante/précédente. Choisissez casse, mots entiers, sélection seulement et retour au début. Le mode étendu reconnaît `\n`, `\r`, `\t` et `\\`. La recherche est littérale ; les expressions régulières ne sont pas implémentées. Sans respect de casse, la normalisation est limitée à ASCII.

Les correspondances sont des surbrillances et repères de minicarte/défilement, pas des modifications. Fermer la recherche conserve les repères ; leur effacement retire la requête. Tout remplacer forme une seule transaction d’annulation validée dans le document courant et refuse les résultats tronqués.

## Chercher dans tous les documents ouverts {#results}

Ctrl+Shift+F cherche dans tous les textes ouverts, y compris sans fichier, non enregistrés et en lecture seule. Il ne parcourt pas récursivement un dossier de projet. Choisissez un résultat pour son aperçu, puis double-cliquez ou ouvrez-le pour activer sa source. Les filtres restreignent chemins et texte. Modifier la source invalide les anciennes positions ; actualisez la recherche.

Les requêtes acceptent jusqu’à 4096 octets et les résultats sont tronqués au-delà de 2000 correspondances. Ctrl+P ouvre la palette des fichiers ; Ctrl+Shift+P celle des actions. Les [filtres de menus](doc:filtering#filters) ont un focus et une requête indépendants.

L’aperçu de la source sélectionnée surligne toutes les occurrences de la requête selon les options de casse et de mot entier. Ces surbrillances suivent les changements de requête et apparaissent aussi dans l’éditeur source ; une ancienne sélection limitée à un document ne restreint pas les repères de recherche globale.


Les [grands fichiers UTF-8](doc:paged-text#search) utilisent une recherche asynchrone et ouvrent les résultats dans leur source paginée. Leur aperçu intégral, recherche dans la sélection et remplacement global ne sont pas fournis.
