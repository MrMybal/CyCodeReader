# CyCodeReader documentation toolchain

The human manual is authored in .CyDocBuild/manual/en and fr. The application
owns its documents, services and Undo; CyDoc supplies the offline reader only.
Generated Documentation/ contains Documentation.html, separate DocAI.md guidance,
DocData resources/notices and an official legacy index.html redirect. Distribute
that complete output, never this source workspace, internal/ or caches.

From the repository root, with Python 3.11+, Node.js 22+, npm and Git:

```text
python .CyDocBuild/build.py
python .CyDocBuild/verify.py Documentation
node .CyDocBuild/verify_dom.mjs Documentation
```

prepare.py builds the exact CyDoc Git revision in cydoc.lock.json with npm ci,
then npm pack, then installs the local archive with the committed dependency lock.
CyDoc is not installed from an npm-registry package. First preparation needs
repository/npm access; the installed cache supports offline generation. No
developer checkout or upstream source is modified.

Only image/gallery modules are selected. Local screenshots and translated media
explanations illustrate actual controls; static diagrams compile to SVG. No
video, GIF, graph, mesh or Three.js runtime is needed. The stock reader provides
top search/Ctrl+K, nested navigation, wider sidebar, tooltips, six themes and
in-window media enlargement/Escape. Browser storage governs reading positions.

English is the original/default. Review both full languages, matching page IDs,
explicit anchors, English revision and French sourceRevision. api.py and the
public cydoc/api-import adapter prepare the 13 operation references from
app/CyTool.json and reviewed api-fr.json translations. The native plugin ABI is
documented against src/PluginABI.h; no implementation/source-file browser is built.
Keep audit/test/migration/build notes in internal/, outside human content.

The build stages CyDoc check (which writes output) in a fresh private directory,
verifies it, then replaces only unchanged owned output. Unknown/local changes
block replacement. Optional repeated --output PATH copies support CMake/Pages.
CMake depends on CyCodeReader_documentation and copies/installs Documentation/;
the Windows packager includes that folder. Native updater path validation accepts
Documentation/ and historical help/ payloads. Core-only builds need no Node.
CYCODE_READER_BUILD_DOCUMENTATION=OFF is a development-only opt-out.

The scripts/ entries are compatibility shims. compose.py combines the verified
Reader export at / with the generated CyCode export at /cycode/ for the existing
Pages project. No source checkout or application data belongs in that artifact.
The GitHub documentation workflow runs the same generation and component checks.

DOM checks disable resource/script loading and network. They check routes,
translation, search, hierarchy, themes, gallery, enlargement and reading state.
Actual file-origin rendering, touch/DPI, native F1 and platform acceptance remain
separate work; record those results privately in internal/.
