#include "Reader.h"
#include "LiveEditor.h"
#include "ModuleModels.h"
#include "cycode/EditorTheme.h"
#include "cycode/EditorLanguage.h"
#include <imgui_internal.h>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>

using namespace cycode;
using namespace cycode::reader;
static void require(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
static void frame(const std::function<void()> &draw,ImGuiKey key=ImGuiKey_None) {
    auto &io=ImGui::GetIO();
    if (key!=ImGuiKey_None) io.AddKeyEvent(key,true);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Fixture",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDecoration);
    draw();ImGui::End();ImGui::Render();
    require(ImGui::GetDrawData()->TotalVtxCount>0,"A migrated view emitted no drawing data");
    if (key!=ImGuiKey_None) io.AddKeyEvent(key,false);
}
static void commands() {
    std::vector<cyui::Command> rows{{uiId("save"),"Save document","Ctrl+S","File"},
                                  {uiId("disabled"),"Disabled command","","File",false},
                                  {uiId("open"),"Open file","Ctrl+O","File"}};
    require(cyui::rankCommands(rows,"document save")==std::vector<size_t>{0},"Multi-word command search");
    cyui::CommandState state;bool open=true;cyui::ItemId request=0;
    auto draw=[&] { request=cyui::commandPalette("Commands###stable",open,state,rows); };
    frame(draw);frame(draw);ImGui::GetIO().AddInputCharactersUTF8("disabled");frame(draw);
    require(state.query=="disabled","Palette text input did not receive the query");
    frame(draw,ImGuiKey_Enter);require(open && !request,"Disabled command executed");
    frame(draw);frame(draw,ImGuiKey_Escape);frame(draw);open=true;frame(draw);frame(draw);
    ImGui::GetIO().AddInputCharactersUTF8("open");frame(draw);frame(draw,ImGuiKey_Enter);
    require(!open && request==uiId("open"),"Enter did not dispatch the stable command ID");
    frame(draw);open=true;frame(draw);frame(draw);frame(draw,ImGuiKey_Escape);
    require(!open && !request,"Escape did not cancel the palette");frame(draw);
    rows.clear();for (size_t i=0;i<120;++i) rows.push_back({1000+i,"Command "+std::to_string(i),"","File"});
    open=true;frame(draw);frame(draw);
    for (int i=0;i<35;++i) { frame(draw,ImGuiKey_DownArrow);frame(draw); }
    frame(draw,ImGuiKey_Enter);
    require(request==1035 && !open,"Palette arrows did not scroll and select a clipped row");frame(draw);
}
static void pluginOptions(const std::filesystem::path &directory) {
    std::filesystem::create_directories(directory);
    const Json manifest={{"abi",1},{"id","fixture"},{"name","Fixture"},{"version","1"},{"commands",Json::array({{{"id","sort"},{"title","Sort"},{"operation","sortLines"}}})},
        {"options",Json::array({{{"id","enabled"},{"type","boolean"},{"default",true}},
                               {{"id","count"},{"type","integer"},{"default",12}},
                               {{"id","label"},{"type","string"},{"default","original"}}})}};
    auto configured=manifest;
    configured["languages"]=Json::array({{{"id","config"},{"name","Fixture configuration"},{"extensions",Json::array({".cycfg"})},{"keywords",Json::array({"option"})},{"lineComment","#"}}});
    std::ofstream(directory/"fixture.json")<<configured.dump();
    Plugins plugins;plugins.load(directory);
    require(plugins.describe().at("errors").empty(),"Option fixture failed to load");
    PluginOptionsView view;view.synchronize(plugins.describe().at("plugins").at(0));
    auto property=[&](const char *name) { return uiId(std::string("plugin/fixture/option/")+name); };
    require(view.properties.size()==3 && property("count")!=property("label"),"Stable property identities");
    const auto revision=view.revision;view.synchronize(plugins.describe().at("plugins").at(0));
    require(view.revision==revision,"Unchanged model reset an active edit");
    require(!view.commit(plugins,"fixture",{{property("label"),{cyui::EditPhase::update,std::string("draft")}},
                                          {property("count"),{cyui::EditPhase::cancel,std::int64_t(100)}}}),"Draft/cancel mutated options");
    require(plugins.describe().at("plugins").at(0).at("options").at("label")=="original","Draft escaped into model");
    require(!view.validate(property("label"),std::string(4097,'a')).empty(),"Oversized string accepted");
    require(!view.validate(property("count"),std::int64_t(2147483648ll)).empty(),"Int32 overflow accepted");
    require(view.commit(plugins,"fixture",{{property("count"),{cyui::EditPhase::commit,std::int64_t(42)}}}),"Commit was not applied");
    view.synchronize(plugins.describe().at("plugins").at(0));
    require(view.revision>revision && plugins.describe().at("plugins").at(0).at("options").at("count")==42,"Option revision did not advance");
    // A native/MCP replacement during an active inspector gesture must cancel the draft.
    view.state.revision=view.revision;
    view.state.drafts[property("label")]={std::string("draft"),std::string("original"),true};
    plugins.configure("fixture",{{"label","external"}});view.synchronize(plugins.describe().at("plugins").at(0));
    std::vector<cyui::PropertyEdit> edits;
    frame([&] { edits=cyui::inspector("Options",view.state,view.properties,view.revision,
                                     [&](auto id,const auto &v) { return view.validate(id,v); }); });
    require(std::any_of(edits.begin(),edits.end(),[&](const auto &e) {
        return e.id==property("label") && e.edit.phase==cyui::EditPhase::cancel;
    }),"External revision did not cancel the stale inspector draft");
    view.commit(plugins,"fixture",edits);
    require(plugins.describe().at("plugins").at(0).at("options").at("label")=="external","Stale option overwrote external data");
    // Use actual pointer/text/Escape input on a property draft, then click away to commit.
    const auto field=*std::find_if(view.properties.begin(),view.properties.end(),
                                   [&](const auto &p) { return p.id==property("label"); });
    cyui::InspectorState interactive;ImRect rect;bool sawUpdate=false,sawCancel=false,sawCommit=false;
    auto draw=[&] {
        const auto proposals=cyui::inspector("Interactive",interactive,{&field,1},view.revision);
        rect=ImGui::GetCurrentContext()->LastItemData.Rect;
        for (const auto &p : proposals) {
            sawUpdate|=p.edit.phase==cyui::EditPhase::update;
            sawCancel|=p.edit.phase==cyui::EditPhase::cancel;
            sawCommit|=p.edit.committed();
        }
        view.commit(plugins,"fixture",proposals);
    };
    frame(draw);frame(draw);
    auto &io=ImGui::GetIO();io.AddMousePosEvent(rect.Max.x-30,rect.Min.y+8);io.AddMouseButtonEvent(0,true);frame(draw);
    io.AddMouseButtonEvent(0,false);frame(draw);io.AddInputCharactersUTF8("draft");frame(draw);
    require(sawUpdate,"Property text input was not activated");
    require(!sawCommit,"Property typing committed before the gesture finished");
    frame(draw,ImGuiKey_Escape);frame(draw);
    require(sawCancel && !sawCommit && plugins.describe().at("plugins").at(0).at("options").at("label")=="external",
            "Escape committed a property draft");
    io.AddMousePosEvent(rect.Max.x-30,rect.Min.y+8);io.AddMouseButtonEvent(0,true);frame(draw);
    io.AddMouseButtonEvent(0,false);frame(draw);io.AddInputCharactersUTF8("commit");frame(draw);
    io.AddMousePosEvent(1400,1000);io.AddMouseButtonEvent(0,true);frame(draw);
    io.AddMouseButtonEvent(0,false);frame(draw);
    require(sawCommit && plugins.describe().at("plugins").at(0).at("options").at("label")!="external",
            "Leaving an edited property did not commit the model");
}
static void editor() {
    cyui::CodeEditor common;configureCodeEditor(common);
    auto &native=common.native();native.SetLanguage(editorLanguage("Actor.h",true));
    const std::string before="\tFVector café;\n",after="\tFVector changed;\n";
    common.setText(before);native.SetInsertSpacesOnTabs(true);
    Core core;auto live=core.add();live->document.replace(before);core.refresh();
    auto revision=live->revision;auto undo=native.GetUndoIndex();
    core.dispatch("documents.replace",{{"id",live->id},{"expectedRevision",revision},{"text",after}});
    require(pullLiveDocument(native,*live,revision,undo),"MCP edit did not reach common CodeEditor");
    frame([&] { common.draw("Common editor",cyui::dp(0,260)); });
    require(common.text()==after,"Common editor lost tabs, UTF-8 or exact final newline");
    native.Undo();require(common.text()==before,"Native edit was not one Undo transaction");
    native.Redo();require(common.text()==after,"Redo changed bytes");
    common.setReadOnly(true);native.SelectAll();native.ReplaceTextPreservingTabs("blocked");
    require(common.text()==after,"Read-only common editor accepted a replacement");
    common.setReadOnly(false);
    frame([&] { common.draw("Common editor",cyui::dp(0,260)); });
    require(native.GetPalette().get(TextEditor::Color::function)==editorColours("").get(TextEditor::Color::function),"Common draw discarded extended token roles");
    common.setText("no final newline");require(common.text()=="no final newline","Final newline was invented");
    cyui::CodeDocument json;require(json.setText("{\"value\": 42}",cyui::CodeLanguage::json,7),"JSON view preparation");
    const cyui::SourceSpan span{0,0,1,cyui::TokenRole::keyword};
    require(!json.setOverlays({&span,1},6),"Stale source overlay accepted");
    cyui::CodeViewState view;frame([&] { cyui::codeViewer("Plugin output",view,json,cyui::dp(0,200)); });
}
static void logs() {
    cyui::LogBuffer buffer(3,256);cyui::LogState state;
    for (int i=0;i<8;++i) buffer.append(cyui::LogLevel::warning,"event "+std::to_string(i),"SDK");
    frame([&] { cyui::logView("SDK logs",state,buffer,cyui::dp(0,200)); });
    require(state.rows.size()==3 && state.rows.front().text=="event 5","Service log is not bounded");
    state.query="event 7";frame([&] { cyui::logView("SDK logs",state,buffer,cyui::dp(0,200)); });
    require(state.filtered.size()==1,"Log search/filter failed");
    buffer.clear();frame([&] { cyui::logView("SDK logs",state,buffer,cyui::dp(0,200)); });
    require(state.rows.empty(),"Clearing logs left stale rows");
}
int main() {
    const auto directory=std::filesystem::temp_directory_path()/
        ("cyreader-modules-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ImGui::CreateContext();
    try {
        auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1500,1100};io.DeltaTime=1.f/60;
        io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
        cyui::apply();cyui::loadFonts();unsigned char *pixels;int width,height;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(ImTextureID(1));
        commands();pluginOptions(directory/"plugins");editor();logs();
        // Draw the actual application views, with no SDL init or native OS window.
        for (const auto *theme : {"graphite","mid-grey","light-grey","amber-night","violet","navy"}) {
            Reader reader({directory,{},theme,"en",true},nullptr);
            io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(ImTextureID(1));
            reader.execute("new");reader.testAction({{"command","setText"},{"text","UFUNCTION() void Trace();\n"}});
            reader.execute("undo");reader.execute("redo");
            require(reader.state().at("documents").at(0).at("text")=="UFUNCTION() void Trace();\n","Reader Undo/Redo through common editor failed");
            reader.execute("plugin",{{"plugin","fixture"},{"command","sort"}});
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
            const auto catalogue=cyui::codeLanguages();
            require(std::count_if(catalogue.begin(),catalogue.end(),[](const auto &item) { return item.builtin; })==101,"Reader did not expose the complete shared catalogue");
            require(cyui::codeLanguageForPath("test.cycfg")!=cyui::CodeLanguage::plain,"Reader did not register its plugin language");
            for (const auto *mode : {"log","hlsl","glsl","shell","powershell","yaml","toml","php","swift","ruby","unreal","fixture/config"}) {
                reader.execute("syntax",{{"language",mode}});
                frame([&] { reader.frame(); });
                require(reader.state().at("documents").at(0).at("syntax")==cyui::codeLanguageInfo(cyui::codeLanguageFromName(mode)).name,"Reader selected a different syntax");
            }
            reader.execute("syntax",{{"language","auto"}});
            const auto logPath=directory/"build.log";std::ofstream(logPath)<<"LogTemp: Error: shader failure\nWarning: fallback\nNormal message\n";
            reader.open(logPath);require(reader.state().at("documents").back().at("syntax")=="Logs","Reader did not select the log engine from the file extension");
            reader.execute("close");

#endif
            reader.execute("plugins");reader.execute("options");reader.execute("mcp");reader.execute("searchOpen");
            for (auto density : {cyui::Density::compact,cyui::Density::comfortable}) for (float dpi : {1.f,1.5f,2.f}) {
                cyui::setDensity(density);cyui::applyDpi(dpi);applyEditorTheme(theme);
                for (int i=0;i<3;++i) frame([&] { reader.frame(); });
                require(reader.state().at("error").get<std::string>().empty(),"Actual migrated Reader view reported an error");
                require(std::abs(cyui::dp(10.f)-10.f*dpi)<.01f,"DPI geometry did not scale");
            }
        }
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
        require(cyui::codeLanguageForPath("test.cycfg")==cyui::CodeLanguage::plain,"Reader left a callback registered after destruction");
#endif
        ImGui::DestroyContext();std::filesystem::remove_all(directory);
        std::cout<<"CyUI: actual Reader views, six themes, two densities, 100/150/200% DPI, keyboard palette, inspector revisions, shared editor transactions and bounded logs passed without OS windows\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';ImGui::DestroyContext();std::filesystem::remove_all(directory);return 1;
    }
}
