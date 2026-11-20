// Compile-only while native execution is suspended. Opt-in fixture, no OS window.
#include "Reader.h"
#include "DiffView.h"
#include "cycode/CodeTokenSlice.h"
#include <cyui/modules/Diff.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace cycode::reader {
struct ReaderSyntaxTests {
    static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
    static void catalogue(){
        for(const auto &path:{"demo.zig","build.zig","BUILD.ZIG","demo.zon","build.zig.zon"}){
            const auto selected=cyui::codeLanguageForPath(path);
            require(selected== (std::string(path).ends_with("zon")?cyui::CodeLanguage::zon:cyui::CodeLanguage::zig),"Zig/ZON association");
        }
        const std::string text="const name: i24 = 0x2a; // été 🛠\nconst msg =\n\\\\raw é 🛠 context\n;\n";
        const auto tokens=cyui::highlightCode(text,cyui::CodeLanguage::zig);
        cyui::CodeDocument document;document.setText(text,cyui::CodeLanguage::zig,7);
        require(document.text()==text && document.language()==cyui::CodeLanguage::zig && document.revision()==7,"CodeDocument lost language/UTF-8");
        auto diff=makeDiffDocument(text,text+"// change\n",cyui::CodeLanguage::zig);
        require(!diff.hunks().empty(),"Zig diff omitted change");
        cyui::DiffDocument unified;unified.setUnified("@@ -1 +1 @@\n-const x = 1;\n+const x = 2;\n",cyui::CodeLanguage::zig);
        const auto start=text.find("raw"),stop=text.find(" context",start);
        auto fragment=text.substr(start,stop-start);auto sliced=cycode::clipCodeTokens(tokens,start,fragment);
        require(!sliced.empty() && sliced.front().role==cyui::TokenRole::string,"Paged raw-string context was reset");
        cycode::validateCodeTokens(fragment,sliced);
        const auto comment=text.find("été");auto commentTokens=cycode::clipCodeTokens(tokens,comment,"été 🛠");
        require(!commentTokens.empty() && commentTokens.front().role==cyui::TokenRole::comment,"Paged comment UTF-8 projection");
        bool refused=false;try{cycode::validateCodeTokens("é",{{1,1,cyui::TokenRole::string}});}catch(...){refused=true;}require(refused,"Split UTF-8 token accepted");
        const auto zon=cyui::highlightCode(".{ .name = .sample, .value = nan, .other = inf, .text = \"été 🛠\" }",cyui::CodeLanguage::zon);
        require(!zon.empty(),"ZON lexer unavailable");
        cyui::CodeLanguageDefinition plugin;plugin.id="reader-test/zig";plugin.name="Test Zig override";plugin.base=cyui::CodeLanguage::zon;plugin.extensions={"zig","zon"};plugin.filenames={"build.zig","build.zig.zon"};
        const auto custom=cyui::registerCodeLanguage(plugin);
        require(cyui::codeLanguageForPath("build.zig")==custom && cyui::codeLanguageForPath("sample.zon")==custom,"Plugin association priority lost");
        require(cyui::codeLanguageFromName(syntaxModeId("Zig"))==cyui::CodeLanguage::zig,"Manual Zig followed plugin extension override");
        cyui::unregisterCodeLanguage(plugin.id);
        require(cyui::codeLanguageForPath("build.zig.zon")==cyui::CodeLanguage::zon,"Plugin removal lost built-in mode");
    }
    static void preferences(const std::filesystem::path &root){
        Json original={{"theme","violet"},{"unknown",{{"keep",true}}},{"syntaxModes",{{syntaxPathKey(root/"future.zig"),"missing-plugin/dialect"},{syntaxPathKey(root/"manual.zon"),"Zig"}}}};
        SyntaxPreferences modes;modes.load(original);
        require(modes.choice(root/"manual.zon")=="zig","Legacy display name was not restored to stable ID");
        require(modes.choice(root/"future.zig")=="missing-plugin/dialect","Unavailable plugin ID lost");
        modes.remember(root/"build.zig","zon");Json settings=original;modes.mergeInto(settings);
        require(settings.at("unknown")==original.at("unknown") && settings.at("theme")=="violet","Unrelated preferences lost");
        SyntaxPreferences restored;restored.load(Json::parse(settings.dump()));
        require(restored.choice(root/"build.zig")=="zon" && restored.choice(root/"future.zig")=="missing-plugin/dialect","Session mode restoration");
        restored.remember(root/"build.zig","auto");restored.mergeInto(settings);require(settings["syntaxModes"][syntaxPathKey(root/"build.zig")]=="auto","Automatic/manual distinction lost");
    }
    static void editor(const std::filesystem::path &root){
        const auto path=root/"build.zig",renamed=root/"renamed.zon";std::ofstream(path)<<"const x: i24 = 42; // été 🛠\n";
        Reader reader({root,{path},"graphite","en",true},nullptr);auto tab=reader.active();
        require(tab && tab->codeEditor.language()==cyui::CodeLanguage::zig,"Opening .zig did not select Zig");
        const auto original=tab->editor.GetText();const auto undo=tab->editor.GetUndoIndex();const auto revision=tab->live->revision;
        tab->editor.SetCursor({0,3});const auto selection=tab->editor.GetMainCursorSelection();
        reader.execute("syntax",{{"language","zon"}});
        require(tab->language=="zon" && tab->codeEditor.language()==cyui::CodeLanguage::zon,"Manual switch");
        require(tab->editor.GetText()==original && tab->editor.GetUndoIndex()==undo && tab->live->revision==revision,"Syntax changed text/Undo/revision");
        require(tab->editor.GetMainCursorSelection().start==selection.start,"Syntax changed selection");
        reader.testAction({{"command","setText"},{"text",original+"// edited\n"}});require(tab->language=="zon","Editing reset manual mode");
        reader.showComparison(original,tab->document.text(),"before","after",0,tab->id,"saved");
        require(reader.comparison_.language==cyui::CodeLanguage::zon,"Diff omitted selected language");
        reader.commitSave({tab->id,renamed,{}});require(tab->language=="zon","Save As discarded manual mode");
        require(reader.syntaxPreferences_.choice(renamed)=="zon","Rename did not transfer preference");
        reader.execute("syntax",{{"language","auto"}});require(tab->language=="auto" && tab->codeEditor.language()==cyui::CodeLanguage::zon,"Automatic after rename");
        reader.execute("syntax",{{"language","missing-plugin/dialect"}});require(tab->language=="missing-plugin/dialect" && tab->codeEditor.language()==cyui::CodeLanguage::plain,"Unavailable manual syntax switched to automatic");
    }
    static int run(const std::filesystem::path &root){
        require(!std::filesystem::exists(root),"Choose a new synthetic fixture directory");std::filesystem::create_directories(root);
        catalogue();preferences(root);ImGui::CreateContext();
        try{ImGui::GetIO().IniFilename=nullptr;editor(root);ImGui::DestroyContext();}
        catch(...){ImGui::DestroyContext();throw;}
        std::cout<<"Zig/ZON catalogue, preferences, editor, diff and UTF-8 fragment fixtures passed.\n";return 0;
    }
};
}
int main(int argc,char **argv){try{return argc==2?cycode::reader::ReaderSyntaxTests::run(cycode::utf8Path(argv[1])):2;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
