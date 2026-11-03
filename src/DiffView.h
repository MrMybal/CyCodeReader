#pragma once
#include <cyui/modules/Diff.h>
namespace cycode::reader {
cyui::DiffDocument makeDiffDocument(const std::string &before,const std::string &after,cyui::CodeLanguage language);
}
