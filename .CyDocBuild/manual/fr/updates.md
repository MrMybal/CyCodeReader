---
id: updates
title: Installer et mettre à jour
description: Utilisez le parcours de l’édition portable ou installée.
section: Utiliser Reader
order: 180
parent: overview
sourceRevision: 4
---

## Éditions {#updates}

Extrayez tout le ZIP portable dans un dossier accessible en écriture et gardez `portable.flag` ; préférences et cache restent dans `data`. L’installateur par utilisateur utilise normalement `%LOCALAPPDATA%/Programs/CyCodeReader` et conserve les préférences séparément. Il ne change pas les associations de fichiers et n’installe pas le SDK Python facultatif. Ne mélangez pas dossiers portable et installé.

Les nouvelles installations publiques vérifient GitHub Releases en arrière-plan au démarrage par défaut. Une version compatible plus récente ouvre le panneau Mises à jour. Utilisez **Télécharger la mise à jour**, puis **Fermer et installer la mise à jour** après avoir examiné la version. Les préférences existantes sont conservées ; **Mises à jour → Vérifier les mises à jour au démarrage** active ou désactive les prochaines vérifications. Le panneau permet aussi une vérification manuelle. Aucun téléchargement de mise à jour séparé n’est à choisir sur GitHub. Les compilations de développement ou de préparation peuvent désactiver ces vérifications. Reader ne collecte pas d’identifiants GitHub privés pour son actualisation. Examinez la version disponible et le téléchargement vérifié par empreinte avant d’accepter.

L’édition portable utilise son ZIP et assistant natif compatibles ; l’édition installée suit son installateur. Enregistrez ou abandonnez les documents et fermez les autres instances si demandé. Annuler préserve le travail en cours. Fermeture, redémarrage et acceptation sont explicites ; ouvrir ce manuel n’installe rien. L’actualisation Linux/macOS n’est pas fournie.

## Récupérer depuis un ancien lanceur {#recovery}

Si Reader 0.1.4 ou 0.1.5 affiche **Invalid update arguments**, l’assistant s’est arrêté avant toute installation. Fermez Reader et lancez manuellement l’installateur Windows actuel à l’emplacement déjà installé une fois. Le lanceur corrigé est inclus à partir de la 0.1.6 ; l’ancien exécutable ne peut pas corriger sa propre transmission en téléchargeant un autre paquet.

Pour une copie portable, fermez Reader puis remplacez ses fichiers de programme par le ZIP portable corrigé complet dans le dossier existant, en conservant `data/` et les plugins personnalisés. Utilisez l’édition correspondante. Les préférences existantes sont conservées. Il n’est pas nécessaire de désinstaller Reader au préalable.
