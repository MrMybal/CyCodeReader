#include "cycode/EditorSnippet.h"
#include "cycode/Snippet.h"
#include "cycode/EditorLanguage.h"
#include <imgui.h>
#include <iostream>
#include <stdexcept>
using namespace cycode;
static void require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
static void frame(TextEditor &editor) {
    ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({850,550});ImGui::Begin("Snippet test");editor.Render("##source");ImGui::End();ImGui::Render();
}
static void key(TextEditor &e,ImGuiKey key,bool shift=false) {
    auto &io=ImGui::GetIO();if (shift) io.AddKeyEvent(ImGuiMod_Shift,true);io.AddKeyEvent(key,true);frame(e);io.AddKeyEvent(key,false);if (shift) io.AddKeyEvent(ImGuiMod_Shift,false);frame(e);
}
static void text(TextEditor &e,const char *value) { ImGui::GetIO().AddInputCharactersUTF8(value);frame(e);frame(e); }
static void acceptance() {
    TextEditor e;EditorSnippet session;session.attach(e);e.SetLanguage(editorLanguage("code.cpp"));e.SetShowMiniMapEnabled(false);e.SetFocus();e.SetInsertSpacesOnTabs(true);
    auto begin=[&](const char *source) {
        const auto s=expandSnippet(source);require(s.has_value(),"Snippet fixture failed");e.SetText(s->text);e.SetFocus();frame(e);session.begin(snippetMetadata(*s,0));frame(e);
    };
    begin("call(${1:value}, $1, ${2|left,right|})$0; tail");
    require(session.active() && session.index()==1 && e.GetNumberOfCursors()==2,"Linked initial selection");
    require(e.GetCursorText(0)=="value" && e.GetCursorText(1)=="value","Field selection content");
    text(e,"🛠");require(e.GetText()=="call(🛠, 🛠, left); tail","Real typing did not update all mirrors");
    key(e,ImGuiKey_Tab);require(session.active() && session.index()==2 && e.GetCursorText(0)=="left","Tab did not select next field");
    session.choose(1);frame(e);require(e.GetText()=="call(🛠, 🛠, right); tail","Choice did not replace active field");
    key(e,ImGuiKey_Tab,true);require(session.index()==1 && e.GetCursorText(0)=="🛠" && e.GetNumberOfCursors()==2,"Shift+Tab lost resized mirrors");
    text(e,"name");require(e.GetText()=="call(name, name, right); tail","Resized mirror text");
    key(e,ImGuiKey_Tab);key(e,ImGuiKey_Tab);require(!session.active() && e.GetNumberOfCursors()==1 && e.GetMainCursorPosition()==TextEditor::DocPos(0,23),"Final $0 caret");
    begin("${1:outer ${2:inner}} $1 ${3:end}$0");
    text(e,"fresh");require(e.GetText()=="fresh fresh end","Replacing outer placeholder");
    key(e,ImGuiKey_Tab);require(session.index()==3,"Replaced nested field remained active");
    key(e,ImGuiKey_Escape);require(!session.active(),"Escape did not finish snippet");
    require(e.IsAutoIndentEnabled(),"Snippet did not restore indentation policy");
    begin("${1:outer ${2:inner}} $1 ${3:end}$0");key(e,ImGuiKey_Tab);text(e,"new");key(e,ImGuiKey_Tab,true);
    require(session.index()==1 && e.GetCursorText(0)=="outer new" && e.GetCursorText(1)=="outer new","Nested edits lost parent mirror ranges");
    begin("${1} / $1 ${2:tail}$0");text(e,"empty");key(e,ImGuiKey_Tab);require(session.index()==2 && e.GetCursorText(0)=="tail","Empty mirrored tab stops");
    begin("${1|a,\t🛠|} $1$0");session.choose(1);frame(e);require(e.GetText()=="\t🛠 \t🛠","Choice converted raw tabs");key(e,ImGuiKey_Escape);e.Undo();e.Redo();require(e.GetText()=="\t🛠 \t🛠" && e.GetNumberOfCursors()==1,"Choice redo changed tabs or revived mirrors");
    begin("${1:name} $1 ${2:end}$0");text(e,"x");e.Undo();require(!session.active() && e.GetText()=="name name end","Undo did not terminate session safely");e.Redo();require(e.GetText()=="x x end" && !session.active() && e.GetNumberOfCursors()==1,"Redo resurrected stale fields");
    begin("${1:name} $1");e.SetText("reload");frame(e);require(!session.active(),"Reload retained snippet anchors");
    begin("${1:name} $1");e.SelectAll();e.ReplaceTextPreservingTabs("external");frame(e);require(!session.active(),"External replacement retained snippet anchors");
    begin("${1:name} $1 ${2:end}");e.SetCursor({0,0});frame(e);require(!session.active() && e.GetNumberOfCursors()==1,"Mouse/API cursor move retained linked edits");
    begin("${1:name} $1 ${2:end}");text(e,"first\nsecond");key(e,ImGuiKey_Tab);require(session.index()==2 && e.GetCursorText(0)=="end","Multiline anchors lost next field");
    e.SetReadOnlyEnabled(true);frame(e);require(!session.active(),"Read-only transition retained snippet");
    e.SetReadOnlyEnabled(false);begin("${1:name} $1");
    size_t requested=0;TextEditor::AutoCompleteConfig config;config.triggerOnTyping=false;config.triggerDelay=std::chrono::milliseconds(0);
    config.callback=[&](TextEditor::AutoCompleteState &state) { ++requested;state.suggestions={"new_completion"}; };e.SetAutoCompleteConfig(&config);
    auto explicitCompletion=[&] {
        const auto before=requested;
        auto &io=ImGui::GetIO();
        auto eventModifier=[&](ImGuiKey mod) {
            // AddKeyEvent receives physical modifiers. Dear ImGui swaps Ctrl /
            // Command for macOS before exposing the logical shortcut modifiers.
            if (io.ConfigMacOSXBehaviors && (mod==ImGuiMod_Ctrl || mod==ImGuiMod_Super))
                return static_cast<ImGuiKey>(mod==ImGuiMod_Ctrl ? ImGuiMod_Super : ImGuiMod_Ctrl);
            return mod;
        };
        for (const auto mod : {ImGuiMod_Ctrl,ImGuiMod_Shift,ImGuiMod_Alt,ImGuiMod_Super})
            if (config.triggerShortcut & mod) io.AddKeyEvent(eventModifier(mod),true);
        const auto key=static_cast<ImGuiKey>(config.triggerShortcut & ~ImGuiMod_Mask_);
        io.AddKeyEvent(key,true);frame(e);
        io.AddKeyEvent(key,false);
        for (const auto mod : {ImGuiMod_Ctrl,ImGuiMod_Shift,ImGuiMod_Alt,ImGuiMod_Super})
            if (config.triggerShortcut & mod) io.AddKeyEvent(eventModifier(mod),false);
        frame(e);frame(e);
        require(!session.active() && requested>before && e.GetNumberOfCursors()==1,
                ("Configured explicit completion did not leave the snippet safely: chord="+
                    std::to_string(config.triggerShortcut)+" requested="+std::to_string(requested)+
                    " before="+std::to_string(before)+" active="+std::to_string(session.active())+
                    " cursors="+std::to_string(e.GetNumberOfCursors())).c_str());
    };
    explicitCompletion();
    begin("${1:name} $1");
    config.triggerShortcut=ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Space;
    e.SetAutoCompleteConfig(&config);
    // Dear ImGui resolves shortcut routing between frames. Register the newly
    // configured chord before injecting it, as in the real preference/input loop.
    frame(e);frame(e);explicitCompletion();
    e.SetAutoCompleteConfig(nullptr);frame(e);
}
int main() {
    ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60;io.Fonts->AddFontDefault();io.Fonts->Build();
    try { acceptance();ImGui::DestroyContext();std::cout<<"Real headless widget: snippet typing/mirrors, Tab/Shift+Tab, choices, Unicode, nesting, undo and lifecycle passed\n";return 0; }
    catch (const std::exception &e) { std::cerr<<e.what()<<'\n';ImGui::DestroyContext();return 1; }
}
