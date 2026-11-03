# Interface captures

These images show the actual CyCodeReader v0.1.1 native views with the graphite
theme, English labels, 100% scaling and a 1280 x 820 viewport.

| File | View |
| --- | --- |
| editor.png | C++ editor, open tabs, toolbar, syntax colors and minimap |
| find-replace.png | Dedicated find/replace window and minimap match markers |
| search-documents.png | Search across two open documents with source preview |
| compare.png | Side-by-side comparison of two versions of the sample C++ file |
| git-history.png | Three sample Git revisions and the selected commit's inline diff |

The source documents and three-commit repository were created for this gallery.
The views were rendered from the application's ImGui draw data with the CPU
capture path used in tests/SoftwareCapture.h, including the original application
logo texture. PNG encoding preserves the rendered pixels without changing UI
content. This records interface layout; desktop and installer acceptance remains
documented separately in ../Validation.md.
