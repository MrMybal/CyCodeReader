# Third-party notices

The owner has not selected a license for CyCodeReader or its shared editor source.
Dependency licenses do not grant a license for this project's original code.

| Component | License | Source |
|---|---|---|
| CyUI | MIT | Owner's CyImgui_Interface checkout, supplied via CYUI_DIR |
| Dear ImGui | MIT | https://github.com/ocornut/imgui |
| TextEditor | MIT | https://github.com/goossens/ImGuiColorTextEdit |
| Tree-sitter and Rust/Java/JS/TS/HTML/CSS grammars | MIT | https://github.com/tree-sitter |
| SDL3 | zlib | https://github.com/libsdl-org/SDL |
| nlohmann/json | MIT | https://github.com/nlohmann/json |
| CyToolsCore 0.10.1 | MIT | Owner's checksum-pinned local SDK wheel |
| MCP Python SDK 1.30.0 | MIT | https://github.com/modelcontextprotocol/python-sdk |

Notices are preserved in licenses/. Exact source revisions and SHA-256 archive
hashes are in dependencies.lock.json. Downloads remain outside Git in runtime/.
editor/scripts/prepare_texteditor.py patches only a build-local widget copy;
original upstream sources and notices remain intact. Windows system fonts are
loaded when available and are not redistributed; other platforms use fallbacks.
Binary distribution still requires a complete transitive notice/package audit.
