#pragma once
#include <TextEditor.h>
#include <memory>
#include <string>
namespace cycode {
// Per-document incremental parser, shared by the IDE and Reader. No LSP required.
class Syntax {
  public:
    Syntax();
    ~Syntax();
    Syntax(const Syntax &)=delete;
    Syntax &operator=(const Syntax &)=delete;
    bool update(TextEditor &editor,const std::string &path,const std::string &text);
    static std::string language(const std::string &path);
    size_t spans() const;
    bool truncated() const;
  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace cycode
