# CyCodeReader 0.1.10

[English](Docs/Releases/v0.1.10.md) · [README français](README.fr.md)

20 novembre 2026 · **BÊTA** · Windows x64.

## Zig et ZON

- Ajout des profils communs CyUI Zig et Zig Object Notation, pour un catalogue de
  104 modes intégrés. Reconnaissance de `.zig`, `build.zig`, `.zon` et
  `build.zig.zon`. La détection automatique garde la priorité des plugins.
- Un même langage dans l’éditeur, les aperçus de recherche et les comparaisons
  de fichiers/onglets/versions enregistrées/historique. Changer la coloration
  préserve texte, révisions, sélection et Annuler ; l’état des diffs est conservé.
- Choix manuels mémorisés par identifiants texte stables, distincts d’Automatique.
  Restauration à la réouverture des fichiers enregistrés, conservation après
  édition et Enregistrer sous. Identifiants de plugins absents et préférences
  sans rapport conservés.
- Filtres natifs et badges Zig/ZON ; Tous les fichiers reste le filtre par défaut.
- Plages lexicales contextualisées découpées sur les fragments UTF-8 paginés,
  avec validation des offsets et frontières de caractères. Cache lexical borné
  existant réutilisé.
- CyUI épinglé à `e8ccec0a96ed820fc46e86059e010efafcef687e`, contenant cette
  implémentation. Core `b902736b0edb9a76365a47778af8a223d50e5b83` et CyMCPSDK 0.7.2
  `e562c731a146d599e3404718ea8414bb5b96b66d` restent inchangés.

Choisissez **Syntaxe → Automatique**, **Zig**, **ZON** ou **Texte brut**. Voir les
[modes et leur persistance](Docs/SyntaxModes.md) et les
[exemples bilingues](examples/zig/README.md).

## Téléchargements

- [Installateur Windows](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.10/CyCodeReader-0.1.10-windows-x64-setup.exe).
- [ZIP portable](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.10/CyCodeReader-0.1.10-windows-x64-portable.zip).
- [Empreintes SHA-256](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.10/SHA256SUMS.txt).

Extrayez le ZIP complet en conservant `data/` et les plugins personnalisés.
Chat natif, CLI/MCP, Free et profils contrôlés/avec jeton restent disponibles.
L’enregistrement reste explicite. La [récupération de l’ancien lanceur](Docs/Releases/v0.1.6.fr.md)
reste applicable à **Invalid update arguments** sur les installations 0.1.4/0.1.5.

## Limites

Il s’agit de coloration hors ligne, sans ZLS, compilateur Zig, débogueur ni analyse
sémantique. Les documents ordinaires restent limités à 4 Mio ; les UTF-8 plus
volumineux utilisent le profil paginé. La coloration contextualisée est limitée
à 32 Mio ; au-delà le texte reste brut sans lecture intégrale pour la coloration.
Automatisation paginée et récupération des buffers non enregistrés, de session
complète et d’historique Annuler restent indisponibles. Les préférences de syntaxe
ne restaurent pas le contenu non enregistré. Voir les [limites paginées](Docs/PagedText.md).

Compilation Windows Release, contrôles de sources/données, documentation locale
et fixtures de régression compilées uniquement ont été vérifiés. L’exécution
native/graphique et de l’installateur reste non testée sous les restrictions
antivirus du poste. Aucun paquet ni validation d’exécution Linux/macOS n’est
fourni. Voir le [périmètre de validation](Docs/Validation.md).
