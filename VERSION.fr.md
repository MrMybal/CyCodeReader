# CyCodeReader 0.1.3

[English](Docs/Releases/v0.1.3.md) · [README français](README.fr.md)

Publication du 6 novembre 2026 · **BÊTA** · Windows x64. Le développement se
poursuit après la première bêta publique ; le canal de mise à jour public ne
constitue pas une déclaration de maturité stable.

## Changements

Les fichiers Verse utilisent le moteur de syntaxe partagé CyUI. `.verse` et
`.digest.verse` sont détectés automatiquement ; Verse est proposé dans le menu
Syntaxe filtrable. L’analyseur natif traite mots-clés, types intégrés, attributs,
spécificateurs, nombres, chaînes/interpolations, commentaires de bloc imbriqués
et commentaires indentés. Les plugins peuvent hériter de Verse et adapter ses
familles de mots et associations. Compilation Verse, connexion UEFN et complétion
par serveur de langage ne sont pas incluses.

Les manuels CyDoc embarqué et en ligne utilisent le lecteur actuel avec guides
anglais/français complets, références API publiques, navigation imbriquée,
recherche dans l’en-tête (Ctrl+K), six thèmes et agrandissement des images/galeries
dans la fenêtre. F1 et l’aide contextuelle utilisent `Documentation/Documentation.html` ;
les anciens liens restent compatibles. Le même export complet est inclus dans
les deux formats Windows.

- [Documentation française](https://mrmybal.github.io/CyCodeReader/Documentation.html#/fr/overview).
- [English documentation](https://mrmybal.github.io/CyCodeReader/Documentation.html#/en/overview).

## Limites connues et validation

La compilation Windows x64 Release, les assertions d’analyse syntaxique évaluées
par le compilateur et les vérifications documentaires ont réussi pour cette
version. L’application native, le compagnon SDK/MCP, le programme de mise à jour
et l’installateur n’ont pas été lancés. Interactions sur le bureau, scénarios
plugins/Undo à l’exécution, installation sur machine vierge et validation
Linux/macOS restent à effectuer.

Extraire le ZIP complet ou utiliser l’installateur Windows par utilisateur. Un
pilote OpenGL 3.3 est requis. Les versions publiques utilisent les vérifications
GitHub existantes sur le canal public ; téléchargement et installation restent
explicites. Les réglages et fichiers utilisateurs conservent leur gestion actuelle.
L’application est GPL-3.0-only ; ses dépendances conservent leurs licences.

Reader reste limité aux fichiers texte de 4 Mio et à la recherche littérale avec
comparaison de casse ASCII. Services de projet, serveurs de langage, recherche
récursive, regex, macros, récupération après plantage et restauration complète
de session/Undo ne sont pas inclus. Les plugins natifs ABI 1 sont du code de
confiance dans le processus et nécessitent un redémarrage ; il n’existe ni place
de marché ni isolation native. Le compagnon privé CyToolsCore est optionnel et
n’est pas fourni avec l’éditeur natif.

Il s’agit de limites documentées, pas d’une liste de bugs par version. Aucune
liste de ce type n’est publiée avec ces notes ; consulter le [suivi des problèmes](https://github.com/MrMybal/CyCodeReader/issues)
pour les signalements.

## Téléchargements et aide

- [ZIP portable Windows x64](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.3/CyCodeReader-0.1.3-windows-x64-portable.zip) · [Installateur par utilisateur](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.3/CyCodeReader-0.1.3-windows-x64-setup.exe).
- [Release et sommes SHA-256](https://github.com/MrMybal/CyCodeReader/releases/tag/v0.1.3).
- L’export complet `Documentation/Documentation.html` et `DocData/` reste
  consultable hors ligne à côté de l’application.
