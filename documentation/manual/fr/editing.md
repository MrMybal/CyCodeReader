---
id: editing
title: Édition et fichiers
description: Édition et fichiers — manuel local de l’application
section: Guides
order: 20
sourceRevision: 1
---

## Documents et transactions {#documents}

Le contenu actif appartient au moteur natif. Les révisions et l’état du disque sont contrôlés avant l’enregistrement ; un changement extérieur peut imposer une revue ou Save as. Undo/Redo concerne les modifications de l’éditeur. Une commande d’aide ne crée pas de transaction d’édition.

## Encodages et lisibilité {#encoding}

CyCodeReader propose UTF-8, UTF-8 BOM, UTF-16 LE/BE et Windows-1252, ainsi que LF, CRLF et CR. Le changement de format reste une modification à enregistrer. Les fichiers non décodables ne doivent pas être convertis silencieusement.

## Fenêtres, zoom et thèmes {#layout}

Déplacez les vues détachables vers un autre écran. Les contrôles de taille de texte et de thème appartiennent à l’application. La restauration des dispositions et de l’historique complet reste soumise aux limites décrites dans les références.

```diagram
{"type": "sequence", "title": "Save / Enregistrer", "actors": [{"id": "ui", "label": "Editor"}, {"id": "core", "label": "Native owner"}, {"id": "disk", "label": "Disk"}], "messages": [{"from": "ui", "to": "core", "label": "Live revision + text"}, {"from": "core", "to": "disk", "label": "Check baseline"}, {"from": "core", "to": "disk", "label": "Guarded atomic save"}, {"from": "core", "to": "ui", "label": "Result / conflict", "reply": true}]}
```
