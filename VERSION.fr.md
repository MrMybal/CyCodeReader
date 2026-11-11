# CyCodeReader 0.1.5

[English](Docs/Releases/v0.1.5.md) · [README français](README.fr.md)

11 novembre 2026 · **BÊTA** · Windows x64.

## Lisibilité du code

- JetBrains Mono Regular 2.304 est fournie sans modification sous SIL OFL 1.1
  pour les éditeurs, aperçus de source et comparaisons, avec sa notice de licence.
- Ctrl + molette règle la vue de code survolée de 50 à 300 % ; les colonnes d’un
  diff partagent le même zoom. Le texte et l’historique d’annulation sont préservés.
- La palette inspirée de Rider utilise des chaînes orange, commentaires verts,
  mots-clés bleus, types mauves et fonctions turquoise selon chaque syntaxe.
  Les annotations TODO gardent le vert ; le contraste du thème ajuste la nuance.
- Les guides multilignes suivent les accolades ; les listes de paramètres n’ont
  plus de trait vertical traversant leurs noms. La minicarte reste disponible.

## Recherche et fermeture

- Les aperçus de recherche globale surlignent les occurrences selon la requête,
  la casse et les mots entiers. L’éditeur reçoit aussi les repères correspondants.
- Une ancienne sélection de recherche locale ne limite plus les repères globaux.
- Une fermeture acceptée masque les fenêtres avant le nettoyage des tâches.
  L’annulation SDK/mises à jour commence immédiatement et l’attente fixe de deux
  secondes du SDK est supprimée. Enregistrer, abandonner et annuler gardent leurs
  contrôles sur les documents non enregistrés.

## Téléchargements

- [Installateur Windows](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.5/CyCodeReader-0.1.5-windows-x64-setup.exe).
- [ZIP portable](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.5/CyCodeReader-0.1.5-windows-x64-portable.zip).
- [Empreintes SHA-256](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.5/SHA256SUMS.txt).
- [Manuel français](https://mrmybal.github.io/CyCodeReader/Documentation.html#/fr/editing#code-text).

La vérification des mises à jour publiques reste activée dans ces paquets.
Les installations utilisent Setup, les copies portables leur ZIP ; aucun paquet
de mise à jour séparé n’est nécessaire.

## Sources, vérifications et limites

Cette version verrouille le commit CyUI publié
`6173630ad5eb1e585eee1a0e7d3617285563e774`, propriétaire de la police, du zoom,
des couleurs et de la correction des guides. Les sources Reader correspondantes
sont disponibles au tag `v0.1.5`, avec verrous et notices des dépendances.

La compilation Windows x64 Release, l’intégrité des paquets, les vérifications
source/données et les exports CyDoc/vitrine bilingues sont contrôlés localement.
Les scénarios natifs sont vérifiés par MSVC /Zs seulement. L’application,
l’installateur, l’assistant et les tests natifs n’ont pas été lancés. Délai réel
de fermeture, molette/DPI/police, installation et mise à jour en ligne restent
à vérifier, ainsi que Linux/macOS. OpenGL 3.3 est requis. Les captures existantes
précèdent cette mise à jour de police et de palette.
