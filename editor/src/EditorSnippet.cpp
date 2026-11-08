#include "cycode/EditorSnippet.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <optional>
#include <map>
#include <stdexcept>
namespace cycode {
using Json=nlohmann::json;
void EditorSnippet::attach(TextEditor &editor) {
    editor_=&editor;
    editor.SetInputHandler([this] { return input(); });
    editor.SetResetHandler([this] { cancel(); });
    editor.SetTransactionCallback([this](const auto &changes) { changed(changes); });
}
void EditorSnippet::cancel() {
    if (active() && editor_) { editor_->CollapseUndoCursors(initialUndo_);editor_->SetAutoIndentEnabled(autoIndent_); }
    fields_.clear();current_=0;
    if (editor_ && editor_->GetNumberOfCursors()>1) editor_->SetSelections({editor_->GetMainCursorSelection()});
}
void EditorSnippet::begin(const Json &metadata) {
    cancel();if (metadata.is_null()) return;
    if (!editor_ || editor_->IsReadOnlyEnabled() || metadata.at("positionEncoding")!="utf-8-bytes") throw std::runtime_error("Invalid editor snippet context");
    const auto text=editor_->GetText();std::vector<Field> fields;size_t occurrences=0;uint32_t previous=0;
    const auto &source=metadata.at("fields");if (!source.is_array() || source.size()>128) throw std::runtime_error("Too many snippet fields");
    // Resolve every requested byte boundary in one pass over the document.
    std::map<size_t,Pos> positions;positions.emplace(metadata.at("final").get<size_t>(),Pos{});
    for (const auto &field : source) {
        const auto &ranges=field.at("ranges");if (!ranges.is_array() || ranges.size()>128) throw std::runtime_error("Too many snippet mirrors");
        occurrences+=ranges.size();if (occurrences>256) throw std::runtime_error("Too many snippet mirrors");
        for (const auto &range : ranges) { positions.emplace(range.at("start").get<size_t>(),Pos{});positions.emplace(range.at("end").get<size_t>(),Pos{}); }
    }
    size_t byte=0;Pos position;
    for (auto &[target,p] : positions) {
        if (target>text.size() || (target<text.size() && (static_cast<unsigned char>(text[target])&0xc0)==0x80)) throw std::runtime_error("Invalid snippet UTF-8 boundary");
        while (byte<target) { const auto c=text[byte++];if (c=='\n') { ++position.line;position.index=0; } else if ((static_cast<unsigned char>(c)&0xc0)!=0x80) ++position.index; }
        p=position;
    }
    occurrences=0;
    for (const auto &entry : source) {
        Field field;field.index=entry.at("index");if (field.index<=previous || field.index>65535) throw std::runtime_error("Invalid snippet field order");previous=field.index;
        const auto &ranges=entry.at("ranges");if (!ranges.is_array() || ranges.empty() || ranges.size()>128) throw std::runtime_error("Invalid snippet mirrors");
        std::string value;
        for (const auto &range : ranges) {
            const auto a=range.at("start").get<size_t>(),b=range.at("end").get<size_t>();if (a>b || b>text.size()) throw std::runtime_error("Invalid snippet range");
            const auto start=positions.at(a),end=positions.at(b);
            if (!field.ranges.empty() && (start<field.ranges.back().end || (start==field.ranges.back().start && end==field.ranges.back().end))) throw std::runtime_error("Overlapping snippet mirrors");
            const auto content=text.substr(a,b-a);if (field.ranges.empty()) value=content;else if (content!=value) throw std::runtime_error("Inconsistent snippet mirrors");
            auto parents=range.value("parents",std::vector<uint32_t>{});if (parents.size()>16) throw std::runtime_error("Invalid snippet parents");
            field.ranges.push_back({start,end,std::move(parents)});if (++occurrences>256) throw std::runtime_error("Too many snippet mirrors");
        }
        field.choices=entry.at("choices").get<std::vector<std::string>>();if (field.choices.size()>64) throw std::runtime_error("Too many snippet choices");
        for (const auto &choice : field.choices) { if (choice.size()>8192) throw std::runtime_error("Snippet choice exceeds 8 KiB");validateEditorText(choice); }
        fields.push_back(std::move(field));
    }
    final_=positions.at(metadata.at("final"));fields_=std::move(fields);initialUndo_=undo_=editor_->GetUndoIndex();
    if (active()) { autoIndent_=editor_->IsAutoIndentEnabled();editor_->SetAutoIndentEnabled(false);select(); }
}
void EditorSnippet::select() {
    std::vector<TextEditor::DocSelection> selections;for (const auto &range : fields_.at(current_).ranges) selections.emplace_back(range.start,range.end);
    editor_->SetSelections(selections);
}
bool EditorSnippet::validCursors() const {
    if (!active() || editor_->IsReadOnlyEnabled() || editor_->GetUndoIndex()!=undo_) return false;
    const auto &ranges=fields_.at(current_).ranges;if (ranges.size()!=editor_->GetNumberOfCursors()) return false;
    std::optional<Pos> relativeStart,relativeEnd;
    for (size_t i=0;i<ranges.size();++i) {
        const auto &range=ranges[i];const auto cursor=editor_->GetCursorSelection(i);
        if (cursor.start<range.start || range.end<cursor.end) return false;
        auto relative=[&](Pos p) { return Pos(p.line-range.start.line,p.line==range.start.line ? p.index-range.start.index : p.index); };
        const auto a=relative(cursor.start),b=relative(cursor.end);
        if (relativeStart && (*relativeStart!=a || *relativeEnd!=b)) return false;relativeStart=a;relativeEnd=b;
    }
    return true;
}
bool EditorSnippet::input() {
    if (!active()) return false;
    if (!validCursors()) { cancel();return false; }
    const auto &io=ImGui::GetIO();
    // Match the configured backend chord, including macOS defaults and host
    // overrides. Cancel mirrors before allowing the widget to request completion.
    const auto shortcut=editor_->GetAutoCompleteShortcut();
    const auto key=static_cast<ImGuiKey>(shortcut & ~ImGuiMod_Mask_);
    if (key!=ImGuiKey_None && io.KeyMods==(shortcut & ImGuiMod_Mask_) && ImGui::IsKeyPressed(key,false)) { cancel();return false; }
    if (!io.KeyCtrl && !io.KeyAlt && !io.KeySuper && ImGui::IsKeyPressed(ImGuiKey_Escape,false)) { cancel();editor_->SetCursor(editor_->GetMainCursorPosition());return true; }
    if (!io.KeyCtrl && !io.KeyAlt && !io.KeySuper && ImGui::IsKeyPressed(ImGuiKey_Tab,false)) {
        if (io.KeyShift) { if (current_) --current_;select(); }
        else if (++current_>=fields_.size()) { const auto end=final_;cancel();editor_->SetCursor(end); }
        else select();
        return true;
    }
    return false;
}
const std::vector<std::string> &EditorSnippet::choices() const { static const std::vector<std::string> empty;return active() ? fields_.at(current_).choices : empty; }
void EditorSnippet::choose(size_t index) {
    if (!validCursors() || index>=choices().size()) throw std::runtime_error("Snippet choice context changed");
    const auto text=choices()[index];select();editor_->ReplaceTextPreservingTabs(text,true);if (active()) select();
}
void EditorSnippet::changed(const std::vector<TextEditor::Change> &changes) {
    if (!active()) return;
    if (editor_->GetUndoIndex()!=undo_+1) { cancel();return; }
    const auto index=fields_.at(current_).index;
    for (const auto &change : changes) {
        const auto &activeRanges=fields_.at(current_).ranges;
        const auto found=std::find_if(activeRanges.begin(),activeRanges.end(),[&](const Range &r) { return r.start<=change.start && (change.insert ? change.start : change.end)<=r.end; });
        if (found==activeRanges.end()) { cancel();return; }const auto owner=*found;
        auto transform=[&](Pos p,bool right) {
            const auto a=change.start,b=change.end;
            if (p<a || (change.insert && p==a && !right)) return p;
            if (change.insert) return p.line==a.line ? Pos(b.line,b.index+p.index-a.index) : Pos(p.line+b.line-a.line,p.index);
            if (p<=b) return a;
            return p.line==b.line ? Pos(a.line,a.index+p.index-b.index) : Pos(p.line-(b.line-a.line),p.index);
        };
        for (auto &field : fields_) for (auto &range : field.ranges) {
            const bool own=field.index==index && range.start==owner.start && range.end==owner.end;
            const bool parent=std::find(owner.parents.begin(),owner.parents.end(),field.index)!=owner.parents.end() && range.start<=owner.start && owner.end<=range.end;
            const bool child=std::find(range.parents.begin(),range.parents.end(),index)!=range.parents.end();
            if (!change.insert && field.index!=index && !parent && ((range.start<change.end && change.start<range.end) || (child && change.start<=range.start && range.end<=change.end))) range.removed=true;
            const bool empty=range.start==range.end;
            range.start=transform(range.start,!own && !parent);range.end=transform(range.end,own || parent || empty);
        }
        final_=transform(final_,true);
    }
    for (auto &field : fields_) std::erase_if(field.ranges,[](const Range &range) { return range.removed; });
    std::erase_if(fields_,[](const Field &field) { return field.ranges.empty(); });
    const auto selected=std::find_if(fields_.begin(),fields_.end(),[&](const Field &field) { return field.index==index; });
    if (selected==fields_.end()) { cancel();return; }current_=std::distance(fields_.begin(),selected);undo_=editor_->GetUndoIndex();
}
}
