#include "cycode/Syntax.h"
#include "cycode/EditorLanguage.h"
#include <iostream>
#include <stdexcept>
#include <imgui.h>
using namespace cycode;
using Color=TextEditor::Color;
void require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
TextEditor::DocPos position(const std::string &text,const std::string &word) {
    auto at=text.find(word); require(at!=std::string::npos,"Fixture token missing"); size_t line=0,column=0;
    for (size_t i=0;i<at;++i) { if (text[i]=='\n') { ++line; column=0; } else if ((static_cast<unsigned char>(text[i])&0xc0)!=0x80) ++column; }
    return {line,column};
}
void sample(const std::string &path,const std::string &text,std::initializer_list<std::pair<std::string,Color>> checks) {
    TextEditor editor; Syntax syntax;
    editor.SetText(text); editor.SetLanguage(editorLanguage(path));
    require(syntax.update(editor,path,text),"Syntax parser failed"); require(!syntax.truncated() && syntax.spans(),"Syntax result missing or truncated");
    for (const auto &[token,color] : checks) {
        const auto actual=editor.GetTokenColor(position(text,token));
        if (actual!=color) { std::cerr<<path<<" token "<<token<<" actual "<<int(actual)<<" expected "<<int(color)<<'\n'; throw std::runtime_error("Wrong syntax role"); }
    }
    const auto changed="// changed\n"+text;
    editor.SelectAll(); editor.ReplaceTextPreservingTabs(changed);
    require(syntax.update(editor,path,changed),"Incremental syntax parse failed");
    editor.Undo(); require(editor.GetText()==text,"Syntax parsing changed undo text");
    require(syntax.update(editor,path,text),"Undo syntax parse failed");
    std::cout<<path<<": parsed "<<syntax.spans()<<" spans\n";
}
int main() {
    ImGui::CreateContext();
    try {
        sample("code.rs","struct Widget { value: i32 }\nfn main() { let raw = r#\"// text\"#; /* outer /* inner */ end */ println!(\"ok\"); }\n",
            {{"Widget",Color::declaration},{"fn main",Color::keyword},{"// text",Color::string},{"inner",Color::comment},{"println",Color::macro}});
        sample("Code.java","class Widget { String text = \"// text\"; int value() { return 42; } }\n",
            {{"Widget",Color::declaration},{"String",Color::declaration},{"// text",Color::string},{"value()",Color::function},{"42",Color::number}});
        sample("code.js","const pattern = /text/; const message = `hello ${target.name}`; function run() { return 42; }\n",
            {{"const",Color::keyword},{"/text/",Color::string},{"hello",Color::string},{"target",Color::identifier},{"run()",Color::function}});
        sample("code.ts","interface Widget { value: number; }\nfunction run(input: Widget): number { return input.value; }\n",
            {{"interface",Color::keyword},{"Widget",Color::declaration},{"number",Color::keyword},{"run(",Color::function}});
        sample("code.tsx","const element = <Widget title=\"Hello\">{count + 1}</Widget>;\n",
            {{"Widget",Color::declaration},{"Hello",Color::string},{"count",Color::identifier}});
        sample("code.html","<!-- comment -->\n<main title=\"Hello\"><style>.card { color: #fff; }</style><script>function greet() { return 42; }</script></main>\n",
            {{"comment",Color::comment},{"main",Color::declaration},{"title",Color::attribute},{"Hello",Color::string},{"color:",Color::attribute},{"greet()",Color::function}});
        sample("code.css","/* comment */\n.card { color: #fff; margin: 4px; }\n",
            {{"comment",Color::comment},{"card",Color::declaration},{"color:",Color::attribute},{"#fff",Color::number}});
        sample("unicode.ts","const text = \"é🛠\"; function run() { return 1; }\n",{{"run()",Color::function}});
        {
            TextEditor editor; Syntax syntax; editor.SetLanguage(editorLanguage("unicode.ts"));
            for (const auto &text : {std::string("const text = \"é🛠\"; function run() {}\n"),std::string("const text = \"è🛡\"; function run() {}\n"),std::string("const text = \"é🛠\"; function run() {}\n")}) {
                editor.SelectAll(); editor.ReplaceTextPreservingTabs(text);
                require(syntax.update(editor,"unicode.ts",text),"Incremental UTF-8 edit failed");
                require(editor.GetTokenColor(position(text,"run()"))==Color::function,"Incremental UTF-8 edit offset was corrupted");
            }
        }
        ImGui::DestroyContext(); return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n'; ImGui::DestroyContext(); return 1; }
}
