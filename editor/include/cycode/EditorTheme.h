#pragma once
#include <cyui/Core.h>
#include <cyui/modules/CodeEditor.h>
#include <TextEditor.h>
#include <initializer_list>
namespace cycode {
void applyEditorTheme(const std::string &key);
struct SyntaxColours {
    ImU32 keyword, type, number, string, comment, preprocessor, function, macro, attribute;
};
SyntaxColours syntaxColours(const std::string &key);
TextEditor::Palette editorColours(const std::string &key);
// Add the application's extra token roles to the common CodeEditor renderer.
void configureCodeEditor(cyui::CodeEditor &editor, ImFont *font=nullptr);
ImU32 readableText(ImU32 foreground, std::initializer_list<ImU32> backgrounds);
} // namespace cycode
