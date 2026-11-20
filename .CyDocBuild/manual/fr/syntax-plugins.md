---
id: syntax-plugins
title: Ajouter une syntaxe
description: Fournissez des règles lexicales ou un moteur natif de jetons.
section: Utiliser Reader
order: 140
parent: plugins
sourceRevision: 3
---

## Définitions {#definitions}

Un langage est enregistré sous `<plugin-id>/<language-id>` dans le catalogue CyUI partagé. Déclarez extensions, mots-clés, types, identifiants, respect de casse et délimiteurs de commentaires. `lexer:"rules"` est le défaut ; `lexer:"inherit", base:"hlsl"` choisit un moteur du catalogue avec éventuellement `wordLists`. Cela n’importe pas les fichiers UDL XML de Notepad++.

```json
{"id":"config","name":"Configuration","extensions":[".mycfg"],"lexer":"rules","lineComment":"#","keywords":["section","option"],"types":["string","number"]}
```

## Jetons natifs {#tokens}

Déclarez une commande `kind:"syntax"` et référencez-la dans `providerCommand`. Le callback reçoit `text` et `options` et retourne des plages d’octets UTF-8 ordonnées et sans chevauchement. Elles restent dans l’instantané et ne coupent aucun caractère.

```json
{"tokens":[{"start":0,"length":6,"role":"keyword"}]}
```

Les rôles comprennent text, keyword, type, number, string, comment, preprocessor, function, added, removed, heading, warning et error. Les intervalles restent neutres. Après une erreur du fournisseur, l’onglet reste modifiable mais la coloration est désactivée ; choisissez à nouveau la syntaxe après correction pour réessayer. Cela n’ajoute pas de services de langage et ne charge pas de bibliothèque Tree-sitter arbitraire.


## Zig et ZON {#zig-and-zon}

Ouvrez `.zig`, `build.zig`, `.zon` ou `build.zig.zon` en mode Automatique. Reader
utilise les associations CyUI partagées, avec priorité aux plugins. Choisissez
**Syntaxe → Zig** ou **ZON** pour imposer un mode, ou **Texte brut** pour désactiver
la coloration. Automatique et choix manuel sont distincts ; la barre d’état
affiche le mode effectif.

Le choix suit les éditions et Enregistrer sous. Il est restauré à la réouverture
d’un fichier enregistré après redémarrage. Les identifiants des plugins absents
restent mémorisés ; la vue affiche du texte brut et une syntaxe indisponible
jusqu’au retour du plugin. Les autres préférences sont conservées. Ces réglages
n’ajoutent pas de récupération des buffers non enregistrés, de session complète
ou d’historique Annuler.

Zig et ZON réutilisent le lexer commun CyUI. Chaînes/commentaires, entiers de
largeur arbitraire, fonctions intégrées, Unicode et littéraux ZON utilisent la
palette commune. L’éditeur, les aperçus de recherche et les comparaisons de
fichiers/historique partagent le mode choisi. Changer la coloration ne modifie
pas le texte et ne crée pas d’entrée Annuler. Voir le [texte paginé](doc:paged-text#limits)
pour le budget de coloration contextualisée de 32 Mio. Il s’agit de coloration
hors ligne, sans ZLS, compilateur, débogueur ni analyse sémantique.
