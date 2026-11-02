# Reader extensions

Place manifests and optional native libraries in the `plugins` folder beside the
deployed Reader. The build deploys two examples there; source manifests live under
plugins/ and the C++ example under examples/. Restart after adding/changing plugins.
The manager reports loaded commands/languages/options and separate load errors.

A data manifest uses ABI 1, a unique id/name/version, commands[], languages[] and
options[]. See plugins/text-tools.json. It adds full-document sort/trim commands,
a boolean option and `.cycfg` keyword/type/comment coloration. In the shared CyUI catalogue build, definitions add extensions, keywords, types, identifiers,
lineComment, commentStart/commentEnd and caseSensitive. These are lexical language
definitions; a native parser can also return shared syntax tokens as described below.

Options declare id, type (`boolean`, signed 32-bit `integer` or bounded `string`)
and default. GUI and SDK use the same validator. Preferences persist option values.
Manifests are limited to 256 KiB and 64 plugins, commands/languages/options per
plugin. Language definitions have bounded extensions and keyword sets. Duplicate
IDs, malformed definitions, outside-directory libraries and incompatible ABI fail.

For arbitrary native commands, build a platform-specific library exporting
`cycodereader_plugin_v1` and the CyCodeReaderPluginV1 structure from src/PluginABI.h.
The `library` field is a local stem; Windows uses .dll, Linux/macOS .so. JSON UTF-8
strings cross the ABI; no STL, ImGui or ownership of host buffers crosses it.
The returned string is plugin-owned until the next invocation. Calls are serial.
The host validates a returned text field before changing the unsaved live document.
Results without text, such as word counts, appear in the manager and SDK result.

Native plugins are trusted code in the Reader process. No hot reload, isolation,
forced cancellation, remote marketplace, automatic downloader, dependency resolver
or third-party compatibility claim is provided. They must not access ImGui from
command callbacks. The native example is exercised as an actual loaded library in
CTest and through the official MCP SDK.


## Shared CyUI syntax engines

Reader registers each language as `<plugin-id>/<language-id>` in `CyUI::Code`.
The Language menu is generated from that registry, grouped alphabetically; all
101 built-in modes and contributed languages use the same selection and file
associations. No app-owned copy of the built-in list is maintained. Unregistering
a language invalidates cached colors without editing text or adding an Undo step.

The default language contribution is a declarative rule set (`lexer: "rules"`):
keywords, types, identifiers (function role), caseSensitive, lineComment,
commentStart and commentEnd. Set both block delimiters to empty strings to disable
them. For a full existing lexer instead use `lexer: "inherit"`, `base: "hlsl"`
(or another built-in catalogue ID) and optional `wordLists` strings. Word-list
indexes follow the selected Lexilla engine; omitted/empty entries inherit its
defaults. This does not import Notepad++ UDL XML or its plugin ABI.

Native plugins may add a declared command with `kind: "syntax"`, then refer to it
from a language's `providerCommand`. These service commands stay out of the user
action menus. The same versioned C ABI is used, with a UTF-8 JSON request:

```json
{"text":"entity Example # comment","options":{}}
```

The response contains ordered, nonoverlapping raw UTF-8 byte ranges:

```json
{"tokens":[{"start":0,"length":6,"role":"keyword"},
           {"start":15,"length":9,"role":"comment"}]}
```

Supported roles are text, keyword, type, number, string, comment, preprocessor,
function, added, removed, heading, warning and error. Gaps remain neutral.
Ranges cannot split UTF-8 characters or exceed the snapshot. Reader bounds the
response and CyUI validates ranges before displaying them. On provider failure,
Reader reports the error once and disables coloring for that tab; the document
remains editable. Select the syntax again to retry after correcting the provider.
The sample native plugin provides `.cynative` coloring through this exact path;
CTest loads the compiled DLL and checks its colors in the common native editor.

Reader serializes calls under its core mutex, keeps libraries loaded for its
lifetime, and unregisters definitions before destroying services. This is trusted
in-process code: callbacks must return promptly. Hot unload, a sandbox, importing
arbitrary Tree-sitter DLLs and a remote marketplace are not provided. Syntax
plugins do not automatically add language-server completion or compiler services.

The normal build now pins the expanded CyUI catalogue commit in
editor/cyui-source.lock.json; no development flag is required. The explicit
`--cyui-development` option is only for further unpublished local changes.
The dependency commit must be pushed to CyUI before another machine can fetch it.
