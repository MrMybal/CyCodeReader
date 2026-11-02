# CyCodeReader development

Read README.md, Docs/Architecture.md and Docs/Validation.md.
This is the independent native C++20 code/text editor. ReaderCore owns live documents,
plugins and the native bridge. CyToolsCore owns jobs/identities/permissions/MCP.
CyCode language servers, project services, Git and debugger remain separate.

- `editor/` is the single shared source library consumed by CyCode too. Preserve
  its `cycode_*` CMake targets and namespace; incompatible changes need an API bump.
- Use CyUI via CYUI_DIR. Read its docs/AI_GUIDE.md; use palette roles and dp sizes.
  Do not edit CyUI or upstream downloaded sources. Widget patches live in
  editor/scripts/prepare_texteditor.py and modify only a build-local copy.
- Keep file services independent of SDL/ImGui. Check disk baselines and preserve
  the shared UTF-8/newline/atomic-save behavior. Unsaved documents stay in memory.
- Reader owns scripts/write_locales.py and locales/ui.json (EN by default, FR).
- Build: python scripts/fetch_dependencies.py, then python scripts/build.py.
  --core-only needs no CyUI/SDL. CTest editor tests create no OS window.
- Do not launch GUI acceptance while the existing protection issue is unresolved
  or repeatedly open application windows. Do not restore old quarantined binaries.
- Never commit runtime/, data/, downloaded dependencies, binaries, credentials or
  an unselected project license. Run Gitleaks before publishing.
- Windows builds/tests are local evidence. Linux/macOS remain unverified until run.

- Use the actual CyToolsCore SDK; read its Docs/AgentGuide.md, MCP.md, Protocol.md
  and TaskFiles.md. Keep user documents persistent; never stage them as task artifacts.
- GUI and MCP must share ReaderCore and its revision/disk guards. Plugins cross
  the versioned C ABI with UTF-8 JSON only; no STL/ImGui objects across the boundary.
