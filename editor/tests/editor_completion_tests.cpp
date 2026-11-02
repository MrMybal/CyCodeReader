#include "cycode/EditorCompletion.h"
#include "cycode/EditorLanguage.h"
#include <imgui.h>
#include <iostream>
#include <stdexcept>
using namespace cycode;
static void require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
static void frame(TextEditor &editor) {
    ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({850,550});
    ImGui::Begin("Editor");editor.Render("##source");ImGui::End();ImGui::Render();
}
static void acceptance(bool managed) {
    TextEditor editor;editor.SetLanguage(editorLanguage("main.cpp"));editor.SetShowMiniMapEnabled(false);
    const std::string before="// Unicode 🛠\n\tgre\n",after="// import greet\n// Unicode 🛠\n\tgreet(\"🛠\");\n";
    editor.SetText(before);editor.SetInsertSpacesOnTabs(true);editor.SetCursor({1,4});editor.SetFocus();
    size_t requested=0,accepted=0;TextEditor::AutoCompleteConfig config;config.triggerDelay=std::chrono::milliseconds(0);
    config.callback=[&](TextEditor::AutoCompleteState &state) { ++requested;state.suggestions={"greet"}; };
    config.accept=[&](size_t index) { require(index==0,"Wrong selected index");++accepted;return managed; };
    editor.SetAutoCompleteConfig(&config);for (int i=0;i<3;++i) frame(editor);
    auto &io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Space,true);frame(editor);
    io.AddKeyEvent(ImGuiKey_Space,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);for (int i=0;i<3;++i) frame(editor);
    require(requested>0,"Real widget did not request suggestions");
    const auto undo=editor.GetUndoIndex();io.AddKeyEvent(ImGuiKey_Enter,true);frame(editor);io.AddKeyEvent(ImGuiKey_Enter,false);frame(editor);
    require(accepted==1,"Real widget did not intercept Enter acceptance once");
    if (managed) {
        require(editor.GetText()==before && editor.GetUndoIndex()==undo,"Widget inserted before native plan was prepared");
        applyEditorCompletion(editor,before,after,2,13);frame(editor);
        require(editor.GetText()==after && editor.GetUndoIndex()==undo+1,"Completion/import were not one transaction");
        require(editor.GetMainCursorPosition()==TextEditor::DocPos(2,12),"UTF-16 caret did not convert to glyph index");
        editor.Undo();require(editor.GetText()==before && editor.GetUndoIndex()==undo,"Undo failed to restore symbol/import/tabs");
        editor.Redo();require(editor.GetText()==after,"Redo failed to preserve symbol/import/tabs");
        bool conflict=false;try { applyEditorCompletion(editor,before,after,2,13); } catch (...) { conflict=true; }require(conflict,"Obsolete widget baseline accepted");
        require(editor.IsInsertSpacesOnTabs(),"Completion changed keyboard tab policy");
    } else require(editor.GetText()=="// Unicode 🛠\n\tgreet\n","Local fallback insertion broke");
    for (int i=0;i<2;++i) frame(editor);
}
static void exactEndings() {
    TextEditor editor;
    for (const auto &source : {std::string{},std::string("source"),std::string("source\n"),std::string("source\n\n"),std::string("// 🛠\ncode")}) {
        editor.SetText(source);require(editor.GetText()==source,"Loading changed final newline");
        editor.SelectAll();editor.ReplaceTextPreservingTabs(source+"!");require(editor.GetText()==source+"!","Replacing changed final newline");
        editor.Undo();require(editor.GetText()==source,"Undo changed final newline");editor.Redo();require(editor.GetText()==source+"!","Redo changed final newline");
    }
}
int main() {
    ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60;io.Fonts->AddFontDefault();io.Fonts->Build();
    try { acceptance(true);acceptance(false);exactEndings();ImGui::DestroyContext();std::cout<<"Real headless ImGui widget: Enter interception, atomic import undo/redo, Unicode caret, exact newlines and local fallback passed\n";return 0; }
    catch (const std::exception &e) { std::cerr<<e.what()<<'\n';ImGui::DestroyContext();return 1; }
}
