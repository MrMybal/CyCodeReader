---
id: encoding
title: Encodages et fins de ligne
description: Convertissez un document explicitement avant de l’enregistrer.
section: Utiliser Reader
order: 40
parent: editing
sourceRevision: 3
---

## Convertir {#convert}

Le menu Encodage propose UTF-8, UTF-8 BOM, UTF-16 LE BOM, UTF-16 BE BOM ou Windows-1252, ainsi que les fins de ligne LF/CRLF/CR. Le changement reste en mémoire et marque le document modifié. Enregistrez explicitement pour écrire cette représentation.

Utilisez Ouvrir en Windows-1252 pour les anciens fichiers concernés ; Reader ne devine jamais cet encodage. Les entrées invalides et caractères non représentables en sortie sont rejetés, sans remplacement destructeur. Un enregistrement sans modification conserve les octets d’origine ; après édition, les fins de ligne détectées sont conservées sauf changement explicite. Les positions UTF-8 des opérations SDK concernent le texte normalisé en mémoire, pas les octets du fichier UTF-16 d’origine.


Le [profil paginé](doc:paged-text#save) conserve UTF-8/BOM et les fins de ligne natives sans conversion d’encodage.
