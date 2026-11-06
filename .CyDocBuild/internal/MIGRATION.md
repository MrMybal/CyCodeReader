# Reader documentation migration — 2026-11-06

Maintenance record excluded from the human manual/export. The owner authorised
source publication; no separate Reader release/version change is requested.

The previous flat-source importer was replaced by 21 paired task guides, four
service groups and 13 paired operation contracts. English is the source/default;
all French titles, descriptions and media explanations are translated. Five
existing application screenshots and the logo are reused. Only image/gallery
are selected; static diagrams compile to SVG, without graph/mesh/Three runtime.
Implementation/source browsing and imported reports/build instructions were
removed from human navigation/search. Developer instructions remain in Docs.

The official CyDoc local npm-pack distribution is pinned with its Git revision
and dependency lock. Sources/tools live in .CyDocBuild; only Documentation/ is
installed/packaged. Native Help targets Documentation.html. The native updater
accepts the new Documentation prefix as well as historical help payloads.
The shared editor directory/API and ReaderCore services were not changed.

Clean Windows x64 configuration and production Reader compilation passed with
BUILD_TESTING=OFF. The offline export/link/translation and DOM component checks
passed, with network/resource loading disabled. Neither app nor native tests,
SDK/MCP companion or updater was launched. Browser file:// automation is blocked
by policy; real local file-origin/touch/DPI and Linux/macOS remain to be checked.
Published HTTPS readers will be checked after the authorised push/deployment.
Private receipts and Gitleaks results stay under ignored runtime audit folders.
