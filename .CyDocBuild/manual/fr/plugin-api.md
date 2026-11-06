---
id: plugin-api
title: API native des plugins
description: Incluez l’ABI publique et échangez du JSON UTF-8.
section: Utiliser Reader
order: 130
parent: plugins
sourceRevision: 2
---

## Point d’entrée {#contract}

Incluez `PluginABI.h` depuis le dossier d’inclusion `src` de la distribution source correspondante. Compilez une bibliothèque partagée compatible exportant `cycodereader_plugin_v1`. Son type est `const CyCodeReaderPluginV1* (*)(void)` ; elle retourne un descripteur stable appartenant au plugin, avec ABI 1, taille de structure et callback execute. Le champ `library` du manifeste est un nom local sans extension. Le chargeur actuel utilise `.dll` sous Windows et `.so` sous Linux/macOS.

```cpp
#include "PluginABI.h"
const char* execute(const char* command, const char* input_json);
extern "C" const CyCodeReaderPluginV1* cycodereader_plugin_v1();
```

Exportez l’entrée avec le mécanisme de visibilité de la plateforme. `command` est l’identifiant déclaré ; `input_json` est la requête UTF-8 avec texte et options concernés. `execute` retourne du JSON UTF-8 appartenant au plugin jusqu’à son appel suivant. Les appels sont sérialisés. Ne laissez pas traverser d’exception C, ne transmettez pas d’objets STL/ImGui et n’appelez pas l’interface graphique. Retournez un membre `text` seulement pour remplacer un texte ; l’hôte contrôle taille, révision et droit de modification avant application.

## Manifestes et options {#manifest}

Un manifeste déclare `abi`, un `id` unique, `name`, `version` et éventuellement commandes, langages et options. Les options ont un type booléen, entier signé 32 bits ou chaîne bornée et une valeur par défaut. Les bibliothèques restent dans le dossier du plugin. Les doublons et ABI incompatibles sont refusés. Chaque manifeste est limité à 256 Kio et les nombres de plugins et contributions à 64.

```json
{"abi":1,"id":"sample.rules","name":"Sample rules","version":"1.0.0","commands":[{"id":"trim","title":"Trim trailing whitespace","operation":"trimTrailing"}],"options":[{"id":"enabled","type":"boolean","default":true}]}
```

Cet exemple de données seul n’exige pas de point d’entrée natif. Un manifeste natif ajoute `library` et ses commandes déclarées. Les [syntaxes](doc:syntax-plugins#definitions) et [comparaisons](doc:comparison-api#contract) ont des contrats JSON spécialisés.
