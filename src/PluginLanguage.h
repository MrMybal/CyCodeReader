#pragma once
#include <TextEditor.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <cyui/modules/Code.h>
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
namespace cycode::reader {
cyui::CodeLanguageDefinition sharedPluginLanguage(const std::string &id,const nlohmann::json &definition);
std::vector<cyui::CodeToken> pluginSyntaxTokens(const nlohmann::json &result);
}
#endif
namespace cycode::reader {
std::unique_ptr<TextEditor::Language> makePluginLanguage(const nlohmann::json &definition);
}
