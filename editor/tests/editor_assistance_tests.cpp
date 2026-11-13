// Native opt-in regressions. During the AV suspension: compiler /Zs only.
#include <TextEditor.h>
#include "cycode/EditorLanguage.h"
#include <imgui.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
static void require(bool v,const char *m){if(!v)throw std::runtime_error(m);}
static void frame(TextEditor &e){
    ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({850,550});
    ImGui::Begin("Assistance");e.Render("source");ImGui::End();ImGui::Render();
}
static void focus(TextEditor &e){e.SetFocus();for(int i=0;i<3;++i)frame(e);}
static void enter(TextEditor &e){auto &io=ImGui::GetIO();io.AddKeyEvent(ImGuiKey_Enter,true);frame(e);io.AddKeyEvent(ImGuiKey_Enter,false);frame(e);}
static void words(){
    TextEditor e;e.SetLanguage(cycode::editorLanguage("code.cpp",true));
    e.SetText("UCyTraceLibrary a;\nUCyTraceLibrary b;\nUCyTraceSubsystem s;\n// UCyTraceComment\nconst char *x=\"UCyTraceString\";\nint32 count;\n");
    const auto rev=e.GetTextRevision(),undo=e.GetUndoIndex();
    const auto list=e.GetDocumentWordSuggestions("UCyTrac");
    require(!list.empty() && list.front()=="UCyTraceLibrary","Document frequency did not prioritize the repeated type");
    require(std::find(list.begin(),list.end(),"UCyTraceSubsystem")!=list.end(),"Other document identifier omitted");
    require(std::find(list.begin(),list.end(),"UCyTraceComment")==list.end(),"Comment indexed as a code identifier");
    require(std::find(list.begin(),list.end(),"UCyTraceString")==list.end(),"String indexed as a code identifier");
    const auto keywords=e.GetDocumentWordSuggestions("in");
    require(std::find(keywords.begin(),keywords.end(),"int32")!=keywords.end(),"Unreal type was omitted");
    require(e.GetTextRevision()==rev && e.GetUndoIndex()==undo,"Suggestion query edited text");
    e.SetText("UCyTraceReplacement value;");
    const auto fresh=e.GetDocumentWordSuggestions("UCyTrac");
    require(std::find(fresh.begin(),fresh.end(),"UCyTraceLibrary")==fresh.end(),"Reload reused a stale dictionary");
    require(std::find(fresh.begin(),fresh.end(),"UCyTraceReplacement")!=fresh.end(),"Reload lost the new identifier");
}
static void blocks(bool spaces){
    TextEditor e;e.SetLanguage(cycode::editorLanguage("code.cpp",true));e.SetInsertSpacesOnTabs(spaces);
    const std::string indent=spaces ? "    " : "\t";
    const std::string before=indent+"int32 UCyTrac::Test(){}";
    e.SetText(before);e.SetCursor({0,indent.size()+22});
    // Use the actual opener's glyph position instead of a fixed column.
    e.SetCursor({0,before.find('{')+1});focus(e);const auto undo=e.GetUndoIndex();enter(e);
    const auto expected=indent+"int32 UCyTrac::Test()\n"+indent+"{\n"+indent+indent+"\n"+indent+"}";
    require(e.GetText()==expected,"Enter did not produce the requested brace layout and indentation");
    require(e.GetMainCursorPosition()==TextEditor::DocPos(2,indent.size()*2),"Caret is not at the inserted indent end");
    require(e.GetUndoIndex()==undo+1,"Block formatting created extra Undo entries");
    e.Undo();require(e.GetText()==before,"Undo failed to restore the exact original block");
    e.Redo();require(e.GetText()==expected,"Redo changed tabs or indentation");
    e.SetText("Value value{}");e.SetCursor({0,12});focus(e);enter(e);
    require(e.GetText().starts_with("Value value{\n"),"Initializer brace was moved as a function block");
    e.SetText("// note {}");e.SetCursor({0,9});focus(e);enter(e);
    require(e.GetText()=="// note {\n}","Comment braces were interpreted as a code block");
}
static void occurrences(){
    TextEditor e;e.SetText("UCyTraceLibrary value;\nUCyTraceLibrary other;\nUCyTraceLibrary2 skip;\nUCyTraceLibrary third;\n");
    const auto undo=e.GetUndoIndex(),rev=e.GetTextRevision();
    e.SetReaderSearchMatches({{{0,15},{0,20}}},IM_COL32(255,180,0,255));
    e.SetSelections({{{0,0},{0,15}}});frame(e);
    require(e.GetSelectionOccurrenceCount()==2,"Exact other identifier occurrences were not highlighted");
    require(e.GetReaderSearchMatchCount()==1,"Selection marks erased Find marks");
    require(e.GetUndoIndex()==undo && e.GetTextRevision()==rev,"Selection marks edited source or Undo");
    e.SetCursor({0,0});frame(e);require(!e.GetSelectionOccurrenceCount(),"Cleared selection retained occurrence marks");
    e.SetText("éclair éclair éclair2");e.SetSelections({{{0,0},{0,6}}});frame(e);
    require(e.GetSelectionOccurrenceCount()==1,"UTF-8 byte indices were confused with glyph positions");
    e.SetSelectionOccurrencesEnabled(false);frame(e);require(!e.GetSelectionOccurrenceCount(),"Disabled occurrences remain visible");
}
int main(){
    ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60;
    io.Fonts->AddFontDefault();io.Fonts->Build();
    try{words();blocks(true);blocks(false);occurrences();ImGui::DestroyContext();std::cout<<"Editor assistance passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';ImGui::DestroyContext();return 1;}
}
