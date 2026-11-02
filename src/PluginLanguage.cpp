#include "PluginLanguage.h"
#include <algorithm>
#include <cctype>
namespace cycode::reader {
std::unique_ptr<TextEditor::Language> makePluginLanguage(const nlohmann::json &definition) {
    auto language=std::make_unique<TextEditor::Language>(definition.value("base","")=="json" ? *TextEditor::Language::Json() : *TextEditor::Language::Cpp());
    language->name=definition.at("name");language->contextTokenizer={};language->preprocess=0;
    language->caseSensitive=definition.value("caseSensitive",true);language->keywords.clear();language->declarations.clear();language->identifiers.clear();
    auto words=[&](const char *field,auto &destination) { for (const auto &item : definition.value(field,nlohmann::json::array())) { auto word=item.template get<std::string>();if (!language->caseSensitive) std::transform(word.begin(),word.end(),word.begin(),[](unsigned char c) { return char(std::tolower(c)); });destination.insert(std::move(word)); } };
    words("keywords",language->keywords);words("types",language->declarations);words("identifiers",language->identifiers);
    language->singleLineComment=definition.value("lineComment",language->singleLineComment);
    language->commentStart=definition.value("commentStart",language->commentStart);language->commentEnd=definition.value("commentEnd",language->commentEnd);return language;
}
}
