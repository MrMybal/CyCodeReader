# CyCodeReader development

Read README.md, Docs/Architecture.md and Docs/Validation.md.
This is the independent native C++20 code/text editor. ReaderCore is a compatibility
name for CyCode::TextSession; CyCode owns native documents, plugins and transport.
Reader owns graphical editing, its commands, Undo/Redo and presentation. CyToolsCore owns jobs/identities/permissions/MCP.
CyCode language servers, project services, Git and debugger remain separate.

- `editor/` owns the shared graphical adapters; CyCode/common now owns Paths,
  TextFile and Snippet. Do not recreate local copies. Preserve
  the existing `cycode_*` CMake targets and namespace; incompatible changes need an API bump.
- Use the locked official CyUI revision (optional clean CYUI_DIR override). Read
  docs/AI_GUIDE.md and MODULES.md; use explicit module targets, palette roles and dp.
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
- The native integration at c4a013b passed seven tests on Windows/Linux/macOS CI.
  These are headless/service checks, not desktop platform acceptance. Local native
  execution remains suspended during the antivirus investigation.

- Use the actual CyToolsCore SDK; read its Docs/AgentGuide.md, MCP.md, Protocol.md
  and TaskFiles.md. Keep user documents persistent; never stage them as task artifacts.
- GUI and MCP must share ReaderCore and its revision/disk guards. Plugins cross
  the versioned C ABI with UTF-8 JSON only; no STL/ImGui objects across the boundary.

- Read Docs/CyUI-Migration.md before changing UI adapters. Keep one ImGui and one
  shared CyUI TextEditor backend; never link a second native widget implementation.

## GitHub Actions paused by the owner (2026-11-09)

- Do not launch, rerun, enable or add automatically triggered GitHub Actions workflows without new explicit authorization. The owner paused Actions to preserve the account's remaining minutes.
- CyCode, CyCodeIDE and CyCodeReader workflows are manually disabled on GitHub. Keep them disabled. Do not push changes that could trigger Actions; any separately authorized push must preserve this pause.
- Use source/data checks and production-only local builds within the existing antivirus restrictions. Record unexecuted platform/native checks honestly; do not replace CI with local native test launches or change antivirus/billing settings.
