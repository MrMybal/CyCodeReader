---
id: paged-text
title: Grands fichiers UTF-8
description: Modifiez de grands fichiers avec le stockage natif et des pages asynchrones.
section: Utiliser Reader
order: 35
parent: editing
sourceRevision: 2
---

## Ouvrir et naviguer {#paged}

La version 0.1.7, en cours de préparation, ouvre les fichiers de plus de 4 Mio en mode
**Texte paginé UTF-8**. Les installateurs publics 0.1.6 précèdent cette fonctionnalité.
L’import et l’indexation s’effectuent en arrière-plan ; l’onglet annonce l’opération
ou son erreur. UTF-8 invalide, binaire, UTF-16 et Windows-1252 explicite sont refusés
sans conversion de la source. Les petits fichiers et brouillons gardent l’éditeur
ordinaire et leurs choix d’encodage.

Utilisez sélection, saisie, Annuler/Rétablir, Ctrl+C/X/V, Ctrl+A, Ctrl+G et Ctrl+molette.
Aller à la ligne accepte des numéros sur 64 bits. Les fragments horizontaux gèrent
aussi les lignes très longues. La taille du document est indépendante de la page
bornée conservée par la vue. L’index natif est actuellement reconstruit après une
édition ; les grandes modifications ou lignes peuvent prendre du temps. Attendez
la fin d’une opération native annoncée avant de fermer ou d’enregistrer.

## Recherche et presse-papiers {#search}

Ctrl+F et F3/Shift+F3 effectuent une recherche littérale asynchrone, avec options de
casse/mot entier et jusqu’à 2 000 résultats. Ctrl+Shift+F inclut les onglets paginés.
Les résultats sélectionnent la vraie source et utilisent la couleur d’avertissement.
Les résultats globaux paginés ouvrent la source sans la dupliquer dans un aperçu
intégral. La recherche dans la sélection et Tout remplacer sont indisponibles dans
ce profil ; remplacer un résultat sélectionné est possible. La recherche utilise
les octets UTF-8 physiques : utilisez les échappements CRLF/CR étendus si nécessaire.

Une insertion est limitée à 1 Mio après normalisation des fins de ligne ; copier ou
couper exporte au plus 4 Mio par sélection. Une sélection plus grande traversant les
pages permet toujours suppression ou remplacement borné. Couper copie avant de
proposer la suppression. Ces budgets ne limitent pas le document à 4 Mio. Les saisies
annulées par une révision ou un refus restent disponibles pour copie et acquittement
manuels ; elles ne sont jamais rejouées automatiquement.

## Enregistrer tout le document {#save}

Ctrl+S écrit en streaming tout le document natif, pages invisibles comprises.
Enregistrer sous préserve l’original et exige une nouvelle destination : fichiers
existants, destinations ouvertes ailleurs, publication en lecture seule et conflits
disque sont refusés. Le BOM et les octets CRLF/CR/LF non modifiés sont conservés.
Les nouvelles fins de ligne suivent la première convention trouvée dans les 64 Kio
initiaux (LF sinon). La conversion d’encodage reste une commande du profil ordinaire.
Enregistrez explicitement une copie avant d’abandonner un travail utile.

## Limites actuelles {#limits}

Le catalogue commun, avec plugins, Verse, HLSL/GLSL et niveaux de journaux, utilise
un contexte lexical complet jusqu’à 32 Mio. Au-delà, l’indisponibilité de la syntaxe
est annoncée et le texte reste brut ; une page ne reçoit pas un état lexical inventé.
Replis, minicarte, curseurs multiples, snippets/complétion, retour visuel à la ligne
et LSP paginé ne sont pas fournis dans ce profil. Comparaisons intégrales, historique
et transformations de plugins exigent un document ordinaire et y sont conservés.

Les onglets paginés sont des documents natifs locaux ; la session SDK/MCP autonome
n’expose actuellement que les documents ordinaires. La récupération durable après
incident est absente. Éditions non enregistrées et saisies annulées sont volatiles :
enregistrez votre travail utile. Les contrats communs manquants de session/récupération
Core et de coloration CyUI en streaming sont décrits dans le guide développeur des
sources. Voir les [limites générales](doc:limits#limits).
