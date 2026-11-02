#pragma once
#include <TextEditor.h>
#include <nlohmann/json.hpp>
namespace cycode {
// Per-editor interactive state; the native core supplies byte-checked metadata.
class EditorSnippet {
public:
    void attach(TextEditor &);
    void begin(const nlohmann::json &);
    void cancel();
    bool active() const { return !fields_.empty(); }
    uint32_t index() const { return active() ? fields_.at(current_).index : 0; }
    const std::vector<std::string> &choices() const;
    void choose(size_t);
private:
    using Pos=TextEditor::DocPos;
    struct Range { Pos start,end;std::vector<uint32_t> parents;bool removed=false; };
    struct Field { uint32_t index;std::vector<Range> ranges;std::vector<std::string> choices; };
    TextEditor *editor_=nullptr;
    std::vector<Field> fields_;
    size_t current_=0,undo_=0,initialUndo_=0;
    bool autoIndent_=false;
    Pos final_;
    void select();
    bool input();
    bool validCursors() const;
    void changed(const std::vector<TextEditor::Change> &);
};
}
