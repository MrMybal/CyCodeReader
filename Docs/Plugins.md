# Reader extensions

Place manifests and optional native libraries in the `plugins` folder beside the
deployed Reader. The build deploys two examples there; source manifests live under
plugins/ and the C++ example under examples/. Restart after adding/changing plugins.
The manager reports loaded commands/languages/options and separate load errors.

A data manifest uses ABI 1, a unique id/name/version, commands[], languages[] and
options[]. See plugins/text-tools.json. It adds full-document sort/trim commands,
a boolean option and `.cycfg` keyword/type/comment coloration. Language base is
`cpp` or `json`; definitions add extensions, keywords, types, identifiers,
lineComment, commentStart/commentEnd and caseSensitive. These are lexical language
definitions; loading arbitrary Tree-sitter binaries/grammars is not implemented.

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
