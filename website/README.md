# CyCodeReader showcase

Static HTML/CSS and a classic JavaScript bundle. The maintained inputs in this
folder generate a complete local website. The existing Pages workflow builds and
publishes the same showcase together with both documentation readers. No framework, remote font, CDN, analytics or
runtime API is required. The English page is prerendered; a complete French
edition uses `?lang=fr`.

## Build and preview

From the repository root:

```sh
python website/build.py
python website/verify.py dist/website
python website/preview.py --port 4388
```

Open `http://127.0.0.1:4388/` or `http://127.0.0.1:4388/?lang=fr`. The preview serves
only `dist/website` on loopback. Ctrl+C stops it. Python 3.11+ builds the website;
the existing documentation toolchain additionally needs Node.js 22+, npm and Git
when preparing its pinned CyDoc package. No native executable is built or run.

The generated entry is `dist/website/index.html`. Copy the **entire** output
directory and open that file to use the static showcase locally. After a verified
manual has already been generated, `python website/build.py --skip-docs-build`
reuses it. An optional `--output` must select a generated path under `dist/` or
`runtime/`; unknown/modified existing output is preserved and blocks replacement.
No Git repository metadata is needed by the website build.

## Content and resources

- `index.html`: page structure and stable chapter IDs.
- `content.json`: complete EN/FR visitor copy, captions, alternatives and controls.
- `config.json`: verified release/source destinations, version and original asset paths.
- `style.css`, `site.js`: responsive layout, language/menu/gallery controls and image dialog.
- `build.py`, `verify.py`, `preview.py`: generation, artifact validation and local serving.
- `assets/manual.jpg`: capture of the actual CyCodeReader v0.1.3 CyDoc interface.

The other images remain single-sourced in the application's existing folders.
The build copies their unchanged bytes to content-hashed URLs; original files are
not edited or duplicated as maintained website inputs.

| Source | View and example data | Caption |
| --- | --- | --- |
| `../icons/CyCodeReader.png` | Original application artwork | Identity in the header, introduction and footer |
| `../Docs/images/editor.png` | v0.1.1, sample C++ files | The code editor · Tabs, syntax and minimap |
| `../Docs/images/find-replace.png` | v0.1.1, find/replace in the same sample | Find / replace · The active document |
| `../Docs/images/search-documents.png` | v0.1.1, two sample buffers | Open-document search · Results and source preview |
| `../Docs/images/compare.png` | v0.1.1, two versions of ReaderConfig | File comparison · Aligned rows and highlighted changes |
| `../Docs/images/git-history.png` | v0.1.1, three synthetic Git revisions | Git history · Revisions and inline differences |
| `assets/manual.jpg` | Real v0.1.3 CyDoc reader, illustrated tour | CyDoc manual · Navigation, search and illustrated guides |

Both language editions state the capture version and retain the application's
real English UI in screenshots. Their explanations, image alternatives and viewer
controls are translated. `Docs/images/README.md` records the native capture method.
The site describes the published Windows beta and current limits. It does not
claim IDE services or native Linux/macOS packages.

## The actual offline manual

The build uses the application's `.CyDocBuild/` and `Documentation/` export.
It copies that complete reader byte for byte under `manual/<content-hash>/`,
including its classic bootstrap, local binary assets, image/gallery modules,
EN/FR pages and notices. The content directory changes when any exported resource
changes, renewing reader URLs without patching generated CyDoc files.

Every documentation action points to a real local page and selected locale.
`Documentation.html` is a stable entry preserving the query and CyDoc fragment;
historical `index.html#/en/...` and `index.html#/fr/...` routes redirect to the
reader. Reading does not need a server. Source Markdown is never a substitute
for the generated reader. See [the application toolchain](../.CyDocBuild/README.md).

## Static hosting and cache renewal

`dist/website/` is a self-contained static artifact: `index.html`, `assets/`,
`manual/`, `Documentation.html`, `.nojekyll`, `LICENSE.txt`, `COPYRIGHT.txt` and the ownership
manifest. Asset names use SHA-256 content revisions. Relative paths work beneath
a project subpath and after moving the output folder. Revalidate the HTML when
configuring hosting; hashed resources alone cannot refresh an already cached page.

Build inputs, local profiles, SDK wheels, application data, binaries and caches
are excluded. Never publish the source checkout or all of `dist/`/`runtime/`.
The existing `.github/workflows/documentation.yml` generates and verifies the
standalone showcase, then `compose_pages.py` includes the unchanged CyCode manual
from the `cycode-docs` branch under `/cycode/`. It uploads only the verified shared
artifact and deploys through the existing GitHub Pages environment.

Public entry points are <https://mrmybal.github.io/CyCodeReader/> (English),
<https://mrmybal.github.io/CyCodeReader/?lang=fr> (French) and
`Documentation.html#/en/overview` or `Documentation.html#/fr/overview` for the
Reader manual. Historical root `#/en/...` and `#/fr/...` links keep opening Reader
help; the shared artifact also retains root `DocData/` runtime resources and the
separate `DocAI.md` sidecar for older cached reader pages; `/cycode/Documentation.html` retains the older CyCode documentation links.

To reproduce shared composition with an existing verified CyCode export:

```sh
python website/compose_pages.py dist/website PATH_TO_CYCODE_EXPORT runtime/pages-upload
python website/verify.py runtime/pages-upload --with-cycode
```

Choose a fresh output directory. Composition validates both inputs, preserves the
CyCode export byte for byte and rejects unexpected files or changed resources.
`scripts/compose_pages.py` remains available for callers needing the older
documentation-only composition; it is not the showcase deployment entry point.

## Interaction and verification

Run `python website/verify.py dist/website` after a build. It validates local
references, EN/FR strings, original image bytes, content revisions, reader copies,
real documentation targets, licenses and the artifact allowlist. JavaScript syntax:

```sh
node --check website/site.js
```

Review initial screenshot visibility in EN/FR at 1280×720, 1366×768 and 1920×1080;
mobile/tablet reflow; keyboard tabs; menu expanded state; image containment,
Escape/focus/scroll restoration; contextual documentation; default/unsupported
languages and links under a static subpath. Open both the site and its copied
reader through `file://` when the browser environment supports that origin.
Keep captures and acceptance records in ignored working folders, outside the
showcase and human documentation. These checks do not validate native application
runtime behavior, installers or platforms.

## License

The original application, showcase and artwork retain
[GPL-3.0-only](../LICENSE); see [COPYRIGHT](../COPYRIGHT). The complete manual retains
its CyDoc MIT and other dependency notices. It uses image/gallery, with no Three.js
runtime. See [third-party notices](../THIRD_PARTY_NOTICES.md).
