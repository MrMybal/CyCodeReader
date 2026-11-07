# CyCodeReader 0.1.4

[English](Docs/Releases/v0.1.4.md) · [README français](README.fr.md)

7 novembre 2026 · **BÊTA** · Windows x64.

## Installation et mises à jour

- L’installateur Windows par utilisateur devient le téléchargement principal
  des README et de la vitrine. Le ZIP portable complet reste disponible à côté.
- Les nouvelles installations vérifient GitHub Releases en arrière-plan au
  démarrage. Une version disponible ouvre Mises à jour. Téléchargez le paquet
  vérifié, puis choisissez **Fermer et installer la mise à jour** ; les demandes
  d’enregistrement habituelles restent actives.
- Les préférences existantes sont conservées. Activez **Mises à jour → Vérifier
  les mises à jour au démarrage** si la vérification était désactivée.
- Les copies installées téléchargent Setup ; les copies portables prennent le
  ZIP compatible. Reader gère son assistant en interne : aucun paquet « update »
  séparé n’est à choisir.
- Le packaging public refuse les compilations dont la vérification est désactivée.

## Téléchargements

- [Installateur Windows](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.4/CyCodeReader-0.1.4-windows-x64-setup.exe).
- [ZIP portable](https://github.com/MrMybal/CyCodeReader/releases/download/v0.1.4/CyCodeReader-0.1.4-windows-x64-portable.zip).
- [Documentation française](https://mrmybal.github.io/CyCodeReader/Documentation.html#/fr/updates).

## Vérifications et limites

La compilation Windows x64 Release, les tests de politique de packaging et
l’export CyDoc complet sont vérifiés. L’installateur, l’application et l’assistant
de mise à jour n’ont pas été lancés localement. Le parcours complet de mise à jour
en ligne, l’installation sur machine vierge et la validation graphique Linux/macOS
restent à vérifier. Un pilote OpenGL 3.3 est requis. L’application reste en bêta,
avec les limites documentées de l’éditeur et de ses plugins.
