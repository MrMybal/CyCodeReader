#pragma once
#include <TextEditor.h>
#include <string>
namespace cycode {
// A completion and its imports are a single undoable widget transaction.
void applyEditorCompletion(TextEditor &,const std::string &before,const std::string &after,size_t line,size_t utf16Character);
}
