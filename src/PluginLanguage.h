#pragma once
#include <TextEditor.h>
#include <nlohmann/json.hpp>
#include <memory>
namespace cycode::reader {
std::unique_ptr<TextEditor::Language> makePluginLanguage(const nlohmann::json &definition);
}
