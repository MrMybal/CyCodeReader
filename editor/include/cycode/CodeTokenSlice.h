#pragma once
#include <cyui/modules/Code.h>
#include <cstdint>
namespace cycode {
// Contextual source spans -> raw UTF-8 fragment-local spans. This is no lexer.
std::vector<cyui::CodeToken> clipCodeTokens(const std::vector<cyui::CodeToken>&,
                                         std::uint64_t fragmentOffset,std::string_view fragment);
void validateCodeTokens(std::string_view fragment,const std::vector<cyui::CodeToken>&);
}
