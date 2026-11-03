#include "Reader.h"
#include "cycode/Paths.h"
#include "cycode/EditorTheme.h"
#include <imgui_internal.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <functional>

namespace cycode::reader {
struct ReaderFilteringTests {
    static void require(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
    static void frame(const std::function<void()> &draw,ImVec2 mouse={-100,-100},int button=-1,bool down=false,ImGuiKey key=ImGuiKey_None) {
        auto &io=ImGui::GetIO();io.AddMousePosEvent(mouse.x,mouse.y);
        if (button>=0) io.AddMouseButtonEvent(button,down);
        if (key!=ImGuiKey_None) io.AddKeyEvent(key,true);
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Filter fixture",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDecoration);
        draw();ImGui::End();ImGui::Render();
        require(ImGui::GetDrawData()->TotalVtxCount>0,"Filtered view emitted no drawing data");
        if (key!=ImGuiKey_None) io.AddKeyEvent(key,false);
    }
    static std::vector<cyui::FilterMenuMatch> matches(Reader &reader,const char *menu,const char *query) {
        cyui::TextFilter filter;filter.query=query;
        return cyui::filterMenuMatches(reader.menuModel(menu).items,filter);
    }
    static cyui::ItemId single(Reader &reader,const char *menu,const char *query) {
        const auto rows=matches(reader,menu,query);require(rows.size()==1,"Expected one matching menu action");return rows[0].id;
    }
    static void menus(Reader &reader) {
        const auto disabled=matches(reader,"Syntax",".glsl");
        require(disabled.size()==1 && !disabled[0].enabled,"Syntax action was not disabled without a document");
        reader.runMenuAction(reader.menuModel("Syntax"),disabled[0].id);require(reader.tabs_.empty(),"Disabled syntax action ran");
        reader.execute("new");reader.testAction({{"command","setText"},{"text","float4 value;\n"}});
        require(single(reader,"Syntax",".glsl")==disabled[0].id,"Enabling a menu action changed its identity");
        const auto english=single(reader,"View","decrease text");
        reader.execute("view.locale",{{"locale","fr"}});
        require(cyui::locale()=="fr" && single(reader,"View","reduire texte")==english,"French accent folding changed menu action identity");
        const auto nested=matches(reader,"Edit","outils descending");
        require(nested.size()==1 && !nested[0].breadcrumb.empty(),"Filter did not combine translated parent and English child words");
        require(matches(reader,"Syntax",".cycfg").size()==1,"Plugin syntax extension did not appear in the shared menu");
        reader.execute("view.locale",{{"locale","en"}});

        // The real filtered menu renderer dispatches the original application action.
        cyui::TextFilter query;query.query=".glsl";ImVec2 click;
        auto draw=[&] {
            const auto model=reader.menuModel("Syntax");
            const auto requested=cyui::filterMenu("Syntax",query,model.items,{300,false,false});
            const auto rect=ImGui::GetCurrentContext()->LastItemData.Rect;click=rect.GetCenter();
            if (requested) reader.runMenuAction(model,requested);
        };
        frame(draw);frame(draw);frame(draw,click,0,true);frame(draw,click,0,false);
        require(reader.active()->codeEditor.language()==cyui::CodeLanguage::glsl,"Clicking filtered GLSL action selected a different language");
        require(matches(reader,"Syntax",".glsl")[0].checked,"Filtering lost the selected syntax check mark");
        const auto before=reader.state().at("documents");
        query.query="nothing-matches";frame(draw);query.clear();
        require(reader.state().at("documents")==before,"Filtering changed document bytes, dirty state, selection or Undo");

        reader.active()->editor.SetReadOnlyEnabled(true);query.query="UTF-16 BE";
        auto encoding=[&] {
            const auto model=reader.menuModel("Encoding");const auto requested=cyui::filterMenu("Encoding",query,model.items,{300,false,false});
            click=ImGui::GetCurrentContext()->LastItemData.Rect.GetCenter();if (requested) reader.runMenuAction(model,requested);
        };
        const auto encodingBefore=std::string(reader.active()->document.encoding());
        frame(encoding);frame(encoding);frame(encoding,click,0,true);frame(encoding,click,0,false);
        require(std::string(reader.active()->document.encoding())==encodingBefore,"Disabled filtered encoding action modified a read-only document");
        reader.active()->editor.SetReadOnlyEnabled(false);

        // A menu popup focuses its own filter; Escape clears it without a model change.
        bool open=true;auto &popupQuery=reader.menuFilters_["Syntax"];
        auto popup=[&] {
            if (open) { ImGui::OpenPopup("Syntax popup");open=false; }
            if (ImGui::BeginPopup("Syntax popup")) { const auto model=reader.menuModel("Syntax");cyui::filterMenu("Syntax",popupQuery,model.items);ImGui::EndPopup(); }
        };
        frame(popup);frame(popup);frame(popup);ImGui::GetIO().AddInputCharactersUTF8("hlsl");frame(popup);
        require(popupQuery.query=="hlsl","Menu filter did not receive typing on opening");
        frame(popup,{-100,-100},-1,false,ImGuiKey_Escape);require(popupQuery.query.empty(),"Escape did not clear the menu query");
        frame([&] { ImGui::ClosePopupToLevel(0,true); });ImGui::ClearActiveID();
    }
    static void panels(Reader &reader,const std::filesystem::path &directory) {
        const auto path=directory/utf8Path("été")/"shader.hlsl";std::filesystem::create_directories(path.parent_path());std::ofstream(path)<<"float4 Main();\n";reader.open(path);
        const auto recent=single(reader,"File","ete shader");std::reverse(reader.recent_.begin(),reader.recent_.end());
        require(single(reader,"File","ete shader")==recent,"Recent-file reordering changed its menu action ID");
        reader.filesFilter_.query="ETE shader";require(reader.filesFilter_.matches("shader.hlsl",pathUtf8(path)),"Open-file filter did not match the accented parent path");
        reader.filesFilter_.clear();reader.execute("plugins");reader.execute("options");
        frame([&] { reader.extensionPanels(); });
        auto &view=reader.pluginOptions_.at("fixture");const auto id=uiId("plugin/fixture/option/label");
        auto dirtyDraft=[&] { view.state.drafts[id]={std::string("draft"),std::string("original"),true}; };
        dirtyDraft();reader.pluginsFilter_.query="enabled";frame([&] { reader.extensionPanels(); });
        require(!view.state.drafts[id].active && view.state.drafts[id].value==cyui::PropertyValue(std::string("original")),"Hiding an active property did not cancel its draft");
        require(reader.core_.plugins().describe().at("plugins").at(0).at("options").at("label")=="original","Filtering committed a plugin draft");
        dirtyDraft();reader.pluginsFilter_.query="no-such-section";frame([&] { reader.extensionPanels(); });
        require(!view.state.drafts[id].active && view.state.drafts[id].cancelled,"Hiding an entire plugin section did not cancel the host draft");
        dirtyDraft();reader.pluginsPanel_=false;frame([&] { reader.extensionPanels(); });
        require(!view.state.drafts[id].active,"Closing the extension panel left a draft active");
        reader.pluginsPanel_=true;reader.pluginsFilter_.clear();reader.optionsFilter_.query="theme";
        frame([&] { reader.extensionPanels(); });
        cyui::TextFilter propertyFilter;propertyFilter.query="navy";
        cyui::Property property;property.label="Theme";property.value=std::string("navy");
        require(!cyui::propertyMatches(propertyFilter,property,"Options"),"Preference filtering indexed values instead of labels");
        reader.optionsFilter_.clear();

        DiagnosticFilterView filtered;cyui::TextFilter rowsFilter;rowsFilter.query="ete warning";
        std::vector<cyui::Diagnostic> rows{{0x100000001ull,cyui::LogLevel::warning,"logs/été.log","warning detail",12,3},{2,cyui::LogLevel::error,"other.log","error detail",3,1}};
        const auto &selected=filtered.rows(rows,1,rowsFilter);require(selected.size()==1 && selected[0].id==0x100000001ull,"Result filtering lost the stable navigation ID");
        rowsFilter.clear();require(filtered.rows(rows,1,rowsFilter).size()==2,"Clearing the result filter lost rows");
        rows.clear();require(filtered.rows(rows,2,rowsFilter).empty(),"Result filter reused a stale model revision");
        rows={{7,cyui::LogLevel::info,"source/été##asset.log","detail",1,1}};rowsFilter.query="ete asset";
        require(filtered.rows(rows,3,rowsFilter).size()==1,"A literal ## in a source path was treated as a hidden widget ID");


        // Starting an Open dialog must preserve all registered plugin languages.
        const auto mode=cyui::codeLanguageForPath("test.cycfg");require(mode!=cyui::CodeLanguage::plain,"Missing fixture syntax");
        reader.execute("open");require(cyui::codeLanguageForPath("test.cycfg")==mode,"Open dialog removed the contributed language from the menu");
        reader.testAction({{"command","dialogResult"},{"files",Json::array()}});reader.pollDialog();
    }
    static void keyboard(Reader &reader) {
        auto &io=ImGui::GetIO();reader.sidebar_=true;reader.resetLayout_=true;
        for (const auto &tab : reader.tabs_) tab->focus=false;
        for (int n=0;n<3;++n) frame([&] { reader.frame(); });
        for (bool mac : {false,true}) {
            io.ConfigMacOSXBehaviors=mac;reader.filesFilter_.clear();ImGui::ClearActiveID();
            auto *files=ImGui::FindWindowByName("Open files###reader-files");require(files!=nullptr,"Open-files window did not render");ImGui::FocusWindow(files);
            frame([&] { reader.frame(); });frame([&] { reader.frame(); });
            io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl,true);
            frame([&] { reader.frame(); },{-100,-100},-1,false,ImGuiKey_F);
            io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl,false);
            frame([&] { reader.frame(); });frame([&] { reader.frame(); });
            io.AddInputCharactersUTF8("ete shader");frame([&] { reader.frame(); });
            require(reader.filesFilter_.query=="ete shader","Ctrl/Cmd+F did not focus the owning Reader panel filter");
            for (const auto *window : ImGui::GetCurrentContext()->Windows)
                require(!(std::string(window->Name).find("find-replace")!=std::string::npos) || window->LastFrameActive!=ImGui::GetFrameCount(),"Panel filtering opened document find/replace");
            frame([&] { reader.frame(); },{-100,-100},-1,false,ImGuiKey_Escape);
            require(reader.filesFilter_.query.empty(),"Escape did not clear the focused panel filter");
            frame([&] { reader.frame(); });ImGui::ClearActiveID();
        }
        io.ConfigMacOSXBehaviors=false;

        ImGui::ClearActiveID();auto *document=ImGui::FindWindowByName(reader.title(reader.active()).c_str());
        require(document!=nullptr,"Document window did not render");ImGui::FocusWindow(document);
        frame([&] { reader.frame(); });frame([&] { reader.frame(); });
        io.AddKeyEvent(ImGuiMod_Ctrl,true);frame([&] { reader.frame(); },{-100,-100},-1,false,ImGuiKey_F);
        io.AddKeyEvent(ImGuiMod_Ctrl,false);frame([&] { reader.frame(); });
        bool documentFind=false;
        for (const auto *window : ImGui::GetCurrentContext()->Windows) documentFind|=std::string(window->Name).find("find-replace")!=std::string::npos && window->LastFrameActive==ImGui::GetFrameCount();
        require(documentFind,"Ctrl+F in the document was stolen by a panel filter");reader.find_.open=false;
        frame([&] { reader.frame(); });ImGui::ClearActiveID();
        reader.searchPanel_=false;
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiMod_Shift,true);
        frame([&] { reader.frame(); },{-100,-100},-1,false,ImGuiKey_F);
        io.AddKeyEvent(ImGuiMod_Ctrl,false);io.AddKeyEvent(ImGuiMod_Shift,false);frame([&] { reader.frame(); });
        require(reader.searchPanel_,"Ctrl+Shift+F was swallowed by the local filter");
        // Filtering views still draw with shared colors and DPI geometry.
        for (const auto *theme : {"graphite","light-grey"}) for (float dpi : {1.f,2.f}) {
            reader.execute("view.theme",{{"theme",theme}});cyui::applyDpi(dpi);applyEditorTheme(theme);
            reader.optionsFilter_.query="theme";reader.pluginsFilter_.query="enabled";
            frame([&] { reader.frame(); });require(reader.state().at("error")=="","Filtered application view reported an error");
        }
    }
    static int run(const std::filesystem::path &source) {
        const auto directory=std::filesystem::temp_directory_path()/("cyreader-filter-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory/"locales");std::filesystem::create_directories(directory/"plugins");
        std::filesystem::copy_file(source/"locales/ui.json",directory/"locales/ui.json");
        const Json plugin={{"abi",1},{"id","fixture"},{"name","Fixture"},{"version","1"},
            {"commands",Json::array({{{"id","sort"},{"title","Sort lines"},{"operation","sortLines"}}})},
            {"languages",Json::array({{{"id","config"},{"name","Écriture configuration"},{"extensions",Json::array({".cycfg"})},{"keywords",Json::array({"option"})}}})},
            {"options",Json::array({{{"id","enabled"},{"type","boolean"},{"default",true}},{{"id","label"},{"type","string"},{"default","original"}}})}};
        std::ofstream(directory/"plugins/fixture.json")<<plugin.dump();
        ImGui::CreateContext();
        try {
            auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1500,1100};io.DeltaTime=1.f/60;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
            {
                Reader reader({directory,{},"graphite","en",true},nullptr);unsigned char *pixels;int width,height;
                io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(ImTextureID(1));
                menus(reader);panels(reader,directory);keyboard(reader);
            }
            ImGui::DestroyContext();std::filesystem::remove_all(directory);
            std::cout<<"Reader filters: real menu actions, disabled/checked states, French accents, plugin drafts, recent paths, revisioned results, Ctrl/Cmd+F, Escape and themes/DPI passed without OS windows\n";return 0;
        } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';ImGui::DestroyContext();std::filesystem::remove_all(directory);return 1; }
    }
};
}
int main(int argc,char **argv) { return argc==2 ? cycode::reader::ReaderFilteringTests::run(argv[1]) : 1; }
