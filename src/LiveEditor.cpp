#include "LiveEditor.h"
namespace cycode::reader {
bool pullLiveDocument(TextEditor &editor,const LiveDocument &live,uint64_t &revision,size_t &undo) {
    if (revision==live.revision) return false;
    if (editor.GetUndoIndex()!=undo) throw std::runtime_error("RevisionConflict: unsynchronized local editor changes");
    if (editor.GetText()!=live.document.text()) { const bool ro=editor.IsReadOnlyEnabled();editor.SetReadOnlyEnabled(false);editor.SelectAll();editor.ReplaceTextPreservingTabs(live.document.text());editor.SetReadOnlyEnabled(ro); }
    revision=live.revision;undo=editor.GetUndoIndex();return true;
}
}
