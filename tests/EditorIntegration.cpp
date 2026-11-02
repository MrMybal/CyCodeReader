#include "Core.h"
#include "LiveEditor.h"
#include "PluginLanguage.h"
#include <imgui.h>
#include <iostream>
using namespace cycode::reader;
static void require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
int main() {
    ImGui::CreateContext();
    try {
        auto language=makePluginLanguage({{"name","Fixture language"},{"keywords",Json::array({"option"})},{"types",Json::array({"number"})},{"lineComment","#"}});
        TextEditor editor;editor.SetLanguage(language.get());editor.SetText("option number count = 2; # comment\n");
        require(editor.GetTokenColor({0,0})==TextEditor::Color::keyword,"Plugin keyword did not color");
        require(editor.GetTokenColor({0,7})==TextEditor::Color::declaration,"Plugin type did not color");
        require(editor.GetTokenColor({0,25})==TextEditor::Color::comment,"Plugin comment did not color");
        Core core;auto live=core.add();live->document.replace("local\n");core.refresh();editor.SetText(live->document.text());size_t undo=editor.GetUndoIndex();auto revision=live->revision;
        core.dispatch("documents.replace",{{"id",live->id},{"expectedRevision",revision},{"text","MCP 🛠\n"}});
        require(pullLiveDocument(editor,*live,revision,undo) && editor.GetText()=="MCP 🛠\n","Native edit did not reach the real widget");
        editor.Undo();require(editor.GetText()=="local\n","MCP edit was not one undo transaction");live->document.replace(editor.GetText());core.refresh();revision=live->revision;undo=editor.GetUndoIndex();
        editor.SelectAll();editor.ReplaceTextPreservingTabs("unsynchronized\n");core.dispatch("documents.replace",{{"id",live->id},{"expectedRevision",revision},{"text","external\n"}});
        bool rejected=false;try { pullLiveDocument(editor,*live,revision,undo); } catch (...) { rejected=true; }require(rejected && editor.GetText()=="unsynchronized\n","Unsynchronized local edits were overwritten");
        ImGui::DestroyContext();std::cout<<"Real editor: plugin lexical roles, native live-buffer synchronization, one-step undo and conflict refusal passed without OS window\n";return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';ImGui::DestroyContext();return 1; }
}
