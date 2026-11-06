---
id: syntax-plugins
title: Ajouter une syntaxe
description: Fournissez des règles lexicales ou un moteur natif de jetons.
section: Utiliser Reader
order: 140
parent: plugins
sourceRevision: 2
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
