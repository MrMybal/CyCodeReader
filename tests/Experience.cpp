#include "Reader.h"
#include "Search.h"
#include "DiffView.h"
#include "SoftwareCapture.h"
#include "cycode/Paths.h"
#include <imgui_internal.h>
#include <fstream>
#include <iostream>
#include <chrono>

namespace cycode::reader {
struct ReaderExperienceTests {
    static void require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
    static void draw(Reader &r) { ImGui::NewFrame();r.frame();ImGui::Render();require(ImGui::GetDrawData()->TotalVtxCount>0,"No drawing data");require(r.lastError_.empty(),r.lastError_.c_str()); }
    static void search(Reader &r) {
        require(r.minimap_ && !r.sidebar_ && !r.wrap_ && !r.whitespace_,"Requested default editor configuration is missing");
        r.execute("new");r.testAction({{"command","setText"},{"text","alpha Alpha alphabet\nété alpha 🛠\nlast\n"}});auto first=r.active();
        require(!first->editor.IsFindReplaceEnabled(),"Native inline find was not disabled");
        require(first->editor.HasTextContextMenuCallback() && first->editor.HasLineNumberContextMenuCallback(),"Code/line context callbacks are missing");
        r.execute("find");r.searchQuery_="alpha";r.find_.words=true;r.rebuildSearch();
        require(r.searchRows_.size()==3,"Find case/whole-word matching is incorrect");r.updateSearchMarkers(first);
        require(first->editor.GetReaderSearchMatchCount()==3,"Minimap search spans do not match the query");
        first->editor.SetCursor({0,0});r.findMatch();require(first->editor.GetCursorText(0)=="alpha","Next result was not selected");
        r.findMatch();require(first->editor.GetCursorText(0)=="Alpha","Case-insensitive next result failed");
        r.findMatch(true);require(first->editor.GetCursorSelection(0).start==TextEditor::DocPos(0,0),"Previous result skipped the preceding match");
        r.find_.replacement="beta";r.find_.sensitive=true;r.find_.words=true;const auto undo=first->editor.GetUndoIndex();
        r.replaceMatches(true);require(first->document.text()=="beta Alpha alphabet\nété beta 🛠\nlast\n","Replace all changed unmatched words or UTF-8");
        require(first->editor.GetUndoIndex()==undo+1,"Replace all was not one Undo transaction");r.execute("undo");require(first->document.text()=="alpha Alpha alphabet\nété alpha 🛠\nlast\n","Undo did not restore the replacement exactly");
        first->editor.SetReadOnlyEnabled(true);const auto text=first->document.text();r.replaceMatches(true);require(first->document.text()==text,"Replacement edited a read-only document");first->editor.SetReadOnlyEnabled(false);
        r.find_.replacement="";r.replaceMatches(true);require(first->document.text()==" Alpha alphabet\nété  🛠\nlast\n","Empty replacement did not delete exactly the matched words");r.execute("undo");require(first->document.text()==text,"Undo did not restore empty replacements");
        r.searchQuery_=std::string(5000,'x');draw(r);draw(r);require(r.find_.open && !r.find_.message.empty(),"Oversized pasted query made the find window unusable");r.searchQuery_="alpha";r.find_.key.clear();
        r.execute("new");r.testAction({{"command","setText"},{"text","unsaved alpha\n"}});auto second=r.active();
        r.execute("searchOpen");r.searchQuery_="alpha";r.find_.sensitive=false;r.rebuildSearch();require(r.searchRows_.size()==4,"Global find omitted an open unsaved buffer");
        Json stale;for (const auto &[key,match] : r.searchNavigation_) if (match.at("id")==first->id) { stale=match;break; }
        r.testAction({{"command","activate"},{"id",first->id}});r.testAction({{"command","setText"},{"text","changed source\n"}});const auto selectionBefore=first->editor.GetMainCursorSelection();r.navigateSearch(stale);require(r.active_==first->id && first->editor.GetMainCursorSelection().start==selectionBefore.start,"Stale search result navigated to an unrelated document");r.rebuildSearch();require(r.searchRows_.size()==1,"Edited search source left stale results");
        r.erase(second->id);r.rebuildSearch();require(r.searchRows_.empty(),"Closed buffer retained search results");
        r.testAction({{"command","setText"},{"text","one\ntwo\none\ntwo\n"}});r.searchPanel_=false;r.find_.open=true;r.find_.extended=true;r.searchQuery_="one\\ntwo";r.find_.replacement="three\\tfour";r.replaceMatches(true);require(first->document.text()=="three\tfour\nthree\tfour\n","Extended multiline replacement corrupted tabs or newlines");
        r.execute("undo");r.find_.extended=false;r.searchQuery_="one";r.find_.selection=true;r.find_.document=first->id;r.find_.selectionRevision=first->live->revision;r.find_.begin=0;r.find_.end=7;r.find_.replacement="ONE";r.replaceMatches(true);require(first->document.text()=="ONE\ntwo\none\ntwo\n","Selection-only replacement escaped its range");
        r.find_.selection=false;r.find_.replace=true;r.find_.focus=true;
        r.execute("view.locale",{{"locale","fr"}});draw(r);draw(r);test::capture("find-replace");
        r.execute("searchOpen");r.searchQuery_="one";draw(r);draw(r);test::capture("search-documents");r.searchPanel_=false;r.find_.open=true;
        for (const auto *locale : {"en","fr"}) for (const auto *theme : {"graphite","light-grey"}) for (float dpi : {1.f,2.f}) { r.execute("view.locale",{{"locale",locale}});r.execute("view.theme",{{"theme",theme}});cyui::applyDpi(dpi);draw(r);draw(r); }
    }
    static void aligned() {
        cyui::DiffDocument doc;std::string a,b;for (int i=0;i<400;++i) { a+="int value_"+std::to_string(i)+"=1;\n";b+="int value_"+std::to_string(i)+"=2;\n"; }doc.setTexts(a,b,cyui::CodeLanguage::cpp);cyui::DiffState state;
        auto render=[&] { ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1200,700});ImGui::Begin("Scroll fixture");cyui::diffViewer("diff",state,doc);ImGui::End();ImGui::Render(); };
        render();render();test::capture("compare");ImGuiWindow *canvas=nullptr;int children=0;
        for (auto *window : ImGui::GetCurrentContext()->Windows) if (window->LastFrameActive==ImGui::GetFrameCount() && std::string(window->Name).find("aligned-diff-canvas")!=std::string::npos) { canvas=window;++children; }
        require(children==1 && canvas && canvas->ScrollMax.y>0,"Split diff did not use exactly one vertical scrolling canvas");
        ImGui::SetScrollY(canvas,500.f);render();require(state.scroll>0,"Shared diff did not scroll");
        const auto old=state.scroll;ImGui::GetIO().AddMousePosEvent(canvas->Pos.x+100,canvas->Pos.y+80);ImGui::GetIO().AddMouseWheelEvent(0,-3);render();require(state.scroll>=old,"Scrolling over the left column moved backward");
        ImGui::GetIO().AddMousePosEvent(canvas->Pos.x+canvas->Size.x*.75f,canvas->Pos.y+80);ImGui::GetIO().AddMouseWheelEvent(0,-3);render();require(state.scroll>=old,"Scrolling over the right column used a separate viewport");
        state.split=false;state.focus=doc.hunks().back();render();render();require(state.scroll>0,"Unified diff change navigation lost its scroll target");
    }
    static void context(Reader &r) {
        ImGui::GetIO().DisplaySize={1280,820};r.execute("view.locale",{{"locale","fr"}});r.resetLayout_=true;
        std::string source="#include <string>\n\n";for (int i=0;i<300;++i) source+="void function_"+std::to_string(i)+"() {\n    // error marker for search\n    const std::string text = \"CyCodeReader\";\n}\n\n";
        r.testAction({{"command","setText"},{"text",source}});r.searchQuery_="error";r.updateSearchMarkers(r.active());draw(r);draw(r);draw(r);test::capture("default-editor");
        ImGuiWindow *editor=nullptr;for (auto *window : ImGui::GetCurrentContext()->Windows) if (window->LastFrameActive==ImGui::GetFrameCount() && std::string(window->Name).find("reader-text-")!=std::string::npos) editor=window;
        require(editor!=nullptr,"Native editor child was not drawn");
        auto &io=ImGui::GetIO();io.AddMousePosEvent(editor->Pos.x+cyui::dp(180.f),editor->Pos.y+cyui::dp(80.f));io.AddMouseButtonEvent(1,true);draw(r);io.AddMouseButtonEvent(1,false);draw(r);
        bool open=false;for (const auto *window : ImGui::GetCurrentContext()->Windows) if (window->LastFrameActive==ImGui::GetFrameCount() && (window->Flags&ImGuiWindowFlags_Popup)) open=true;
        require(open,"Right-clicking actual code did not open its context menu");test::capture("code-context");
    }
    static int run(const std::filesystem::path &source) {
        const auto root=std::filesystem::temp_directory_path()/("reader-experience-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root/"locales");std::filesystem::copy_file(source/"locales/ui.json",root/"locales/ui.json");
        ImGui::CreateContext();
        try {
            auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1400,1000};io.DeltaTime=1.f/60;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
            { Reader r({root,{},"graphite","en",true},nullptr);test::preloadCaptureGlyphs();unsigned char *pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(ImTextureID(1));search(r);r.find_.open=false;r.searchPanel_=false;cyui::applyDpi(1.f);r.execute("view.theme",{{"theme","graphite"}});aligned();context(r); }
            require(findTextMatches("aaa aaaa a", "a",true,true).matches.size()==1,"Word boundaries failed");require(findTextMatches("a\na\na", "a",true,false,2).truncated,"Search limit was not reported");
            const auto begin=std::chrono::steady_clock::now();findTextMatches(std::string(4*1024*1024,'a'),std::string(4000,'a')+"b",false,false);require(std::chrono::steady_clock::now()-begin<std::chrono::seconds(2),"Adversarial search exceeded its linear-time budget");
            ImGui::DestroyContext();std::filesystem::remove_all(root);std::cout<<"Default layout, dedicated find/replace, open-buffer search, UTF-8/selection/Undo guards, minimap spans and single-canvas diff passed without OS windows.\n";return 0;
        } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';ImGui::DestroyContext();return 1; }
    }
};
}
int main(int argc,char **argv) { return argc==2 ? cycode::reader::ReaderExperienceTests::run(cycode::utf8Path(argv[1])) : 1; }
