# Third-party notices

CyCodeReader's original source, including the shared `editor/` library, is
licensed under [GNU GPL v3 only](LICENSE) (`GPL-3.0-only`).
Copyright (c) 2026 Cyberalien; see [COPYRIGHT](COPYRIGHT).
Dependencies retain their own licenses and copyright notices.

| Component | License | Source |
|---|---|---|
| CyUI | MIT | https://github.com/MrMybal/CyUI (editor/cyui-source.lock.json) |
| Dear ImGui | MIT | https://github.com/ocornut/imgui |
| TextEditor | MIT | https://github.com/goossens/ImGuiColorTextEdit (bundled by pinned CyUI) |
| Tree-sitter and Rust/Java/JS/TS/HTML/CSS grammars | MIT | https://github.com/tree-sitter |
| SDL3 | zlib | https://github.com/libsdl-org/SDL |
| nlohmann/json | MIT | https://github.com/nlohmann/json |
| CyToolsCore 0.10.1 | MIT | Owner's checksum-pinned local SDK wheel |
| MCP Python SDK 1.30.0 | MIT | https://github.com/modelcontextprotocol/python-sdk |

Notices are preserved in licenses/. Exact source revisions and SHA-256 archive
hashes are in dependencies.lock.json; the immutable CyUI Git revision is in
editor/cyui-source.lock.json. Downloads remain outside Git in runtime/.
editor/scripts/prepare_texteditor.py patches only a build-local widget copy;
original upstream sources and notices remain intact. Windows system fonts are
loaded when available and are not redistributed; other platforms use fallbacks.
Binary distribution still requires a complete transitive notice/package audit.


The expanded CyUI Code build includes selected Lexilla engines, Scintilla public
headers and generated SciTE keyword/property data under their common permissive
license. Exact source commits/checksums are in CyUI's
third_party/code-syntax.lock.json. No Scintilla editor or Notepad++ GPL code is
linked. Preserve licenses/Lexilla.txt, Scintilla.txt and SciTE-Properties.txt with
distributions using this build. The native ABI example is Reader source; it does
not wrap a Notepad++ plugin binary.


Local help uses [CyDoc](https://github.com/MrMybal/CyDoc), MIT, pinned by
`.CyDocBuild/cydoc.lock.json`. The exported help includes its MIT notice
and the Marked, PrismJS and esbuild notices in `Documentation/DocData/licenses/`.
Only image/gallery runtime modules are distributed; no Three.js runtime is included.
