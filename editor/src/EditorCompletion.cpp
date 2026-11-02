#include "cycode/EditorCompletion.h"
#include <stdexcept>
namespace cycode {
void applyEditorCompletion(TextEditor &editor,const std::string &before,const std::string &after,size_t line,size_t character) {
    if (editor.IsReadOnlyEnabled() || editor.GetNumberOfCursors()!=1 || editor.GetText()!=before) throw std::runtime_error("RevisionConflict: editor completion context changed");
    size_t at=0;
    for (size_t i=0;i<line;++i) { at=after.find('\n',at);if (at==std::string::npos) throw std::runtime_error("Completion cursor is outside document");++at; }
    size_t units=0,glyphs=0;
    while (units<character && at<after.size() && after[at]!='\n') {
        const auto c=static_cast<unsigned char>(after[at]);units+=c>=0xf0 ? 2 : 1;
        at+=c>=0xf0 ? 4 : c>=0xe0 ? 3 : c>=0xc0 ? 2 : 1;++glyphs;
    }
    if (units!=character || at>after.size()) throw std::runtime_error("Invalid UTF-16 completion cursor");
    editor.SelectAll();editor.ReplaceTextPreservingTabs(after);editor.SetCursor({line,glyphs});
}
}
