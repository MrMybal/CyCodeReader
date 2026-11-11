# CyCodeReader 0.1.7

[English](Docs/Releases/v0.1.7.md) · [README français](README.fr.md)

11 novembre 2026 · **BÊTA — préparation locale, non publiée** · Windows x64.

## Core natif partagé et grands fichiers UTF-8

- Core `f5e6261bf8acf22d0b3241622ed4294322724fb9` et CyUI
  `0ba1ba663b4b2bf46d066a32829bc91ea2743cba`, API source 1.
- Documents, encodages, recherche, plugins texte, processus, transport et
  utilitaires communs fournis par Core plutôt que dupliqués dans Reader.
- Fichiers UTF-8 de plus de 4 Mio ouverts avec stockage natif et pages asynchrones.
  Édition, Annuler/Rétablir, recherche littérale, navigation 64 bits et sauvegarde
  intégrale s’effectuent en arrière-plan. Enregistrer sous préserve l’original et
  refuse les destinations existantes.
- Saisies refusées ou annulées conservées pour copie et acquittement, y compris
  pendant une proposition native. Le premier CRLF est détecté même après une
  première ligne dépassant 64 Kio. Le BOM et
  les fins LF/CRLF/CR non modifiées sont préservés ; les insertions suivent la
  politique native de fins de ligne.
- Backend graphique injectable pour le propriétaire natif existant de l’IDE,
  avec acquittement des reçus de commit et contrats d’historique natif.
- Correction de mise à jour 0.1.6, JetBrains Mono, Ctrl+molette, palette partagée,
  surlignage de recherche et masquage des fenêtres à la fermeture conservés.

## Limites du profil paginé

Les petits fichiers gardent l’éditeur ordinaire, ses encodages, minimap, repli et
curseurs multiples. Le profil paginé ne fournit pas minimap, repli, multicurseur,
retour automatique, extraits, conversion d’encodage, comparaison intégrale,
Tout remplacer ou recherche dans la sélection. La coloration contextuelle utilise
un instantané complet borné à 32 Mio ; au-delà, le texte reste brut avec un message.
L’import et l’indexation peuvent prendre du temps. L’exposition SDK/MCP et la
récupération durable attendent un contrat natif commun de session ; les travaux
paginés non enregistrés restent volatils. Voir [PagedText](Docs/PagedText.md).

## Distribution et vérification

La compilation locale Release garde les tests natifs désactivés et les mises à
jour publiques activées. ZIP portable, installateur par utilisateur et empreintes
SHA-256 sont préparés localement, sans publication. Les téléchargements publics
restent en 0.1.6. Les contrôles sources/données, compilation seule des scénarios,
paquets et manuel bilingue sont consignés dans [Validation](Docs/Validation.md).

Application, assistant, CLI, installateur et tests natifs ne sont pas exécutés
pendant la suspension liée à l’antivirus. Les parcours interactifs, installation,
mise à jour, backend IDE, Linux et macOS restent à valider. Les Actions restent
désactivées.
