---
id: plugins
title: Gérer les extensions
description: Consultez commandes, options et syntaxes des extensions.
section: Utiliser Reader
order: 120
parent: overview
sourceRevision: 2
---

## Extensions {#extensions}

Ouvrez Extensions pour consulter plugins chargés, commandes, langages, options et erreurs de chargement séparées. Configurez les options dans leurs champs typés ; les valeurs invalides sont refusées. Une commande qui retourne du texte change le document en mémoire sans l’enregistrer. Examinez une transformation avant d’enregistrer un fichier important.

Installez les manifestes et bibliothèques natives facultatives compatibles dans `plugins` près de l’application, puis redémarrez. L’exemple text-tools ajoute tri, suppression d’espaces finaux et syntaxe CyConfig ; l’exemple natif et le fournisseur Git montrent d’autres contributions. Il n’existe ni marché d’extensions, téléchargeur, rechargement à chaud ou bac à sable natif.

## Développement {#development}

Utilisez l’ABI 1 de Reader, pas l’ABI différente de CyCode ni les plugins Notepad++. Consultez le [contrat natif public](doc:plugin-api#contract), les [définitions de syntaxe](doc:syntax-plugins#definitions) et les [fournisseurs de comparaison](doc:comparison-api#contract). Les bibliothèques natives exécutent du code de confiance avec les droits du processus et doivent répondre rapidement.
