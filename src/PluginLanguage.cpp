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

#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
namespace cycode::reader {
cyui::CodeLanguageDefinition sharedPluginLanguage(const std::string &id,const nlohmann::json &source) {
    cyui::CodeLanguageDefinition result;result.id=id;result.name=source.at("name");
    for (const auto &ext : source.at("extensions")) result.extensions.push_back(ext.get<std::string>().substr(1));
    result.base=cyui::codeLanguageFromName(source.value("base","plain"));
    if (source.value("lexer",std::string("rules"))=="inherit") {
        result.wordLists=source.value("wordLists",std::vector<std::string>{});
    } else {
        cyui::CodeSyntaxRules rules;rules.keywords=source.value("keywords",std::vector<std::string>{});
        rules.types=source.value("types",std::vector<std::string>{});rules.functions=source.value("identifiers",std::vector<std::string>{});
        rules.caseSensitive=source.value("caseSensitive",true);rules.lineComment=source.value("lineComment",std::string("//"));
        rules.blockCommentStart=source.value("commentStart",std::string("/*"));rules.blockCommentEnd=source.value("commentEnd",std::string("*/"));result.rules=std::move(rules);
    }return result;
}
std::vector<cyui::CodeToken> pluginSyntaxTokens(const nlohmann::json &result) {
    if (result.contains("error")) throw std::runtime_error(result.at("error").get<std::string>());
    const auto &items=result.at("tokens");if (!items.is_array() || items.size()>200000) throw std::runtime_error("Invalid plugin syntax response");
    static const std::map<std::string,cyui::TokenRole> roles={
        {"text",cyui::TokenRole::text},{"keyword",cyui::TokenRole::keyword},{"type",cyui::TokenRole::type},
        {"number",cyui::TokenRole::number},{"string",cyui::TokenRole::string},{"comment",cyui::TokenRole::comment},
        {"preprocessor",cyui::TokenRole::preprocessor},{"function",cyui::TokenRole::function},
        {"added",cyui::TokenRole::added},{"removed",cyui::TokenRole::removed},{"heading",cyui::TokenRole::heading},
        {"warning",cyui::TokenRole::warning},{"error",cyui::TokenRole::error}};
    std::vector<cyui::CodeToken> tokens;tokens.reserve(items.size());
    for (const auto &item : items) {
        if (!item.at("start").is_number_unsigned() || !item.at("length").is_number_unsigned()) throw std::runtime_error("Syntax offsets must be unsigned byte counts");
        tokens.push_back({item.at("start").get<size_t>(),item.at("length").get<size_t>(),roles.at(item.at("role").get<std::string>())});
    }return tokens;
}
}
#endif
