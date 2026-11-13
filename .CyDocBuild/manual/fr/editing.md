---
id: editing
title: Modifier et enregistrer
description: Travaillez avec les documents en mémoire, l’annulation et les changements externes.
section: Utiliser Reader
order: 30
parent: overview
sourceRevision: 5
---

## Documents {#documents}

Chaque onglet possède un document natif et une révision. L’édition, les curseurs multiples, sélections, indentations, correspondances de délimiteurs et replis agissent sur son texte en mémoire. Annuler/Rétablir restaure les modifications prises en charge ; ouvrir une comparaison ou appliquer un filtre ne modifie pas le document.

L’enregistrement contrôle l’état initial du disque et utilise un remplacement atomique protégé. Un fichier modifié ailleurs provoque un conflit au lieu de remplacer silencieusement les nouveaux octets. Examinez-le avant rechargement ou Enregistrer sous. Recharger peut abandonner du texte non enregistré et demande un choix adapté. Enregistrer sous contrôle les fichiers existants et les autres destinations ouvertes. Un document en lecture seule refuse les modifications.

## Encodage {#encoding}

UTF-8/BOM et UTF-16 LE/BE BOM sont détectés. Windows-1252 reste explicite. Les [commandes d’encodage](doc:encoding#convert) modifient le format en attente ; enregistrer reste séparé. Le profil ordinaire est limité à 4 Mio ; les grands UTF-8 utilisent le [profil paginé](doc:paged-text#paged).

## Disposition {#layout}

Affichage règle les retours visuels, espaces, minicarte, taille du texte et réinitialisation de disposition. Détachez un onglet ou choisissez une nouvelle fenêtre pour obtenir une vraie fenêtre native sur un autre écran. Les [préférences](doc:settings#appearance) règlent thèmes et langue. Reader ne restaure pas une session complète ni un historique de récupération après incident.

## Texte du code {#code-text}

Les vues de code utilisent JetBrains Mono Regular, fournie avec l’application. Maintenez Ctrl et tournez la molette sur un éditeur, un aperçu de recherche ou une comparaison pour régler le texte de cette vue entre 50 et 300 %. Les deux côtés d’une comparaison partagent le même zoom. Les autres panneaux conservent leur taille de texte. Ce réglage visuel ne modifie ni le document ni son historique d’annulation ; le zoom par vue n’est pas restauré entre les sessions. Le rendu des ligatures n’est pas fourni.

Les guides de blocs multilignes suivent les accolades ; les listes de paramètres conservent l’appariement des délimiteurs sans trait vertical traversant leurs noms. La minicarte reste disponible dans Affichage.

## Aides locales au code {#assistance}

La prochaine compilation des sources propose pendant la saisie les identifiants
déjà présents dans le document ordinaire et les mots-clés/types du langage actif.
Ctrl+Espace ouvre la même liste ; Entrée ou Tab valide, Échap ferme. Les identifiants
répétés sont prioritaires. Cette complétion locale ne résout pas les types du projet,
les membres ou les imports. Commentaires et chaînes sont exclus de son index.

Sélectionnez un identifiant entier sur une ligne pour surligner ses autres
occurrences exactes. Ces marques disparaissent avec la sélection et coexistent
avec la recherche, sans modifier le texte ni l’annulation.

En C, C++/Unreal et C#, Entrée après une ouverture de bloc de fonction/contrôle
place l’accolade sur la ligne suivante, indente le corps et aligne la fermeture.
Espaces ou tabulations suivent la politique de l’éditeur ; une annulation restaure
le texte initial. Accolades d’initialisation, commentaires et chaînes ne sont pas
reformatés comme des en-têtes de fonction. Il s’agit des blocs pendant la saisie,
pas d’un passage complet Rider/clang-format. Les fichiers paginés gardent leurs
limites documentées. La 0.1.7 publique précède cette correction des sources. Le
fournisseur de complétion propre à l’application hôte reste prioritaire.

## Fermer Reader {#close}

Reader demande d’enregistrer ou d’abandonner les documents modifiés avant de quitter. Annuler garde l’application ouverte. Une fois la fermeture acceptée, les fenêtres natives sont masquées avant la fin du nettoyage en arrière-plan.
