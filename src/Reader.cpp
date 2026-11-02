#include "Reader.h"
#include "LiveEditor.h"
#include "cycode/EditorLanguage.h"
#include "cycode/EditorTheme.h"
#include "cycode/FileIcons.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <fstream>
#include <cyui/CyUI.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace cycode::reader {
namespace {
bool unrealFile(const std::filesystem::path &path) {
    auto parent=path.parent_path();
    for (int depth=0; depth<8 && !parent.empty(); ++depth) {
        std::error_code ec; int visited=0;
        for (std::filesystem::directory_iterator it(parent,std::filesystem::directory_options::skip_permission_denied,ec),end;
             !ec && it!=end && visited++<256; it.increment(ec)) {
            const auto ext=it->path().extension();
            if (ext==".uproject" || ext==".uplugin") return true;
        }
        const auto next=parent.parent_path(); if (next==parent) break; parent=next;
    }
    return false;
}
bool contains(std::string value,std::string query) {
    auto lower=[](unsigned char c) { return char(std::tolower(c)); };
    std::transform(value.begin(),value.end(),value.begin(),lower);
    std::transform(query.begin(),query.end(),query.begin(),lower);
    return value.find(query)!=std::string::npos;
}
constexpr const char *themes[]={"graphite","mid-grey","light-grey","amber-night","violet","navy"};
constexpr const char *languages[]={"auto","plain","C","C++","C++ / Unreal","C#","Rust","Java","JavaScript","TypeScript","TypeScript / TSX","HTML","CSS","GLSL","HLSL","Python","Lua","JSON","Markdown"};
}
Reader::Reader(Options options,SDL_Window *window) : options_(std::move(options)),core_(options_.installation/"plugins"),window_(window) {
    try { std::ifstream in(options_.installation/"locales/ui.json"); in>>translations_; } catch (...) {}
    if (!options_.automated) try {
        const auto settings=Json::parse(readTextFileBytes(options_.installation/"data/reader/preferences.json"));
        theme_=settings.value("theme",theme_); locale_=settings.value("language",locale_);
        textScale_=std::clamp(settings.value("textScale",textScale_),.8f,2.f);
        wrap_=settings.value("wrap",false); minimap_=settings.value("minimap",false);
        if (settings.contains("recent")) recent_=settings.at("recent").get<std::vector<std::string>>();
        if (settings.contains("pluginOptions")) for (const auto &[id,value] : settings.at("pluginOptions").items()) try { core_.plugins().configure(id,value); } catch (...) {}
        if (recent_.size()>12) recent_.resize(12);
    } catch (...) {}
    if (!options_.theme.empty()) theme_=options_.theme;
    if (!options_.language.empty()) locale_=options_.language;
    if (std::find(std::begin(themes),std::end(themes),theme_)==std::end(themes)) theme_="graphite";
    if (locale_!="en" && locale_!="fr") locale_="en";
    cyui::Config config; config.themeKey=theme_.c_str(); config.squareWindows=true;
    config.dpiScale=SDL_GetWindowDisplayScale(window_); cyui::apply(config);
    applyEditorTheme(theme_); cyui::loadFonts(); ImGui::GetStyle().FontScaleMain=textScale_;
    preparePluginLanguages();
    for (const auto &path : options_.files) open(path);
    if (!options_.automated) startSdk();
}
Reader::~Reader() {
    bridge_.reset();
    if (sdk_.valid()) { if (sdk_.wait_for(std::chrono::seconds(2))!=std::future_status::ready) sdkStop_=true; sdk_.wait(); }
    if (options_.automated) return;
    try {
        const auto path=options_.installation/"data/reader/preferences.json";
        std::filesystem::create_directories(path.parent_path());
        std::optional<std::string> baseline;
        if (std::filesystem::exists(path)) baseline=readTextFileBytes(path);
        const auto pluginState=core_.plugins().describe();Json pluginOptions=Json::object();for (const auto &plugin : pluginState.at("plugins")) pluginOptions[plugin.at("id").get<std::string>()]=plugin.at("options");
        writeTextFileAtomic(path,Json{{"pluginOptions",pluginOptions},{"theme",theme_},{"language",locale_},{"textScale",textScale_},
            {"wrap",wrap_},{"minimap",minimap_},{"recent",recent_}}.dump(2),baseline);
    } catch (...) {}
}
const char *Reader::tr(const char *key) const {
    const auto it=translations_.find(key);
    return it!=translations_.end() && it->contains(locale_) ? it->at(locale_).get_ref<const std::string &>().c_str() : key;
}
void Reader::error(const std::exception &exception) { lastError_=exception.what(); cyui::notify(lastError_.c_str(),cyui::Tone::error); }
std::shared_ptr<Reader::Tab> Reader::find(size_t id) const { for (const auto &tab : tabs_) if (tab->id==id) return tab; return {}; }
std::shared_ptr<Reader::Tab> Reader::active() const { return find(active_); }
std::string Reader::title(const std::shared_ptr<Tab> &tab) const {
    const auto name=tab->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->document.path().filename());
    return "     "+name+"###reader-document-"+std::to_string(tab->id);
}
void Reader::remember(const std::filesystem::path &path) {
    const auto text=pathUtf8(path);
    std::erase(recent_,text); recent_.insert(recent_.begin(),text); if (recent_.size()>12) recent_.resize(12);
}
void Reader::setLanguage(const std::shared_ptr<Tab> &tab) {
    const auto &mode=tab->language;
    const TextEditor::Language *language=nullptr;
    tab->syntaxPath=tab->document.untitled() ? "code.cpp" : pathUtf8(tab->document.path());
    if (mode=="auto") language=editorLanguage(tab->syntaxPath,!tab->document.untitled() && unrealFile(tab->document.path()));
    else if (mode=="C++ / Unreal") language=editorLanguage("code.cpp",true);
    else if (mode!="plain") {
        const std::pair<const char *,const char *> mapping[]={{"C","code.c"},{"C++","code.cpp"},{"C#","code.cs"},{"GLSL","code.glsl"},{"HLSL","code.hlsl"},{"Python","code.py"},{"Lua","code.lua"},{"JSON","code.json"},{"Markdown","code.md"},{"Rust","code.rs"},{"Java","code.java"},{"JavaScript","code.js"},{"TypeScript","code.ts"},{"TypeScript / TSX","code.tsx"},{"HTML","code.html"},{"CSS","code.css"}};
        for (const auto &[name,file] : mapping) if (mode==name) { language=editorLanguage(file); tab->syntaxPath=file; }
    }
    if (mode=="plain" || mode=="C++ / Unreal") tab->syntaxPath="code.txt";
    if (mode.starts_with("plugin:")) { const auto key=mode.substr(7);if (pluginLanguages_.contains(key)) { language=pluginLanguages_.at(key).get();tab->syntaxPath="code.txt"; } }
    else if (mode=="auto") { const auto ext=tab->document.path().extension().string();if (pluginExtensions_.contains(ext)) { language=pluginLanguages_.at(pluginExtensions_.at(ext)).get();tab->syntaxPath="code.txt"; } }
    tab->editor.SetLanguage(language);
    tab->syntax.update(tab->editor,tab->syntaxPath,tab->document.text());
}
void Reader::install(const std::shared_ptr<Tab> &tab) {
    tab->editor.SetInsertSpacesOnTabs(false); tab->editor.SetText(tab->document.text());
    tab->editor.SetInsertSpacesOnTabs(true); tab->editor.SetTabSize(4); tab->editor.SetLineFoldingEnabled(true);
    tab->editor.SetCompletePairedGlyphs(true); setLanguage(tab);
    core_.refresh();tab->appliedRevision=tab->live->revision;
    tab->version=tab->editor.GetUndoIndex(); tab->diskChanged=false;
}
void Reader::newFile() {
    if (tabs_.size()>=64) throw std::runtime_error(tr("Close a document before opening more files."));
    auto tab=std::make_shared<Tab>(core_.add()); tab->id=tab->live->id; install(tab); tabs_.push_back(tab); active_=tab->id;
}
void Reader::open(const std::filesystem::path &path) {
    std::lock_guard lock(core_.mutex());
    try {
        const auto full=std::filesystem::weakly_canonical(std::filesystem::absolute(path));
        for (const auto &tab : tabs_) if (sameFile(tab->document.path(),full)) { active_=tab->id; tab->focus=true; return; }
        if (tabs_.size()>=64) throw std::runtime_error(tr("Close a document before opening more files."));
        auto tab=std::make_shared<Tab>(core_.add(Document::open(full,openEncoding_))); tab->id=tab->live->id;
        install(tab); tabs_.push_back(tab); active_=tab->id; remember(full);
    } catch (const std::exception &exception) { error(exception); }
}
void Reader::sync(const std::shared_ptr<Tab> &tab) {
    if (!tab) return;
    if (pullLiveDocument(tab->editor,*tab->live,tab->appliedRevision,tab->version)) tab->syntax.update(tab->editor,tab->syntaxPath,tab->document.text());
    if (tab->version==tab->editor.GetUndoIndex()) return;
    try { tab->document.replace(tab->editor.GetText()); tab->syntax.update(tab->editor,tab->syntaxPath,tab->document.text()); }
    catch (const std::exception &exception) { install(tab); error(exception); }
    core_.refresh();tab->appliedRevision=tab->live->revision;
    tab->version=tab->editor.GetUndoIndex();
}
void Reader::showDialog(bool save,size_t tab) {
    if (dialog_) return;
    dialog_=std::make_shared<Dialog>(); dialog_->save=save; dialog_->tab=tab;
    if (const auto doc=find(tab); doc && !doc->document.untitled()) dialog_->initial=pathUtf8(doc->document.path());
    if (options_.automated) return; // Scripted selection uses pollDialog; native dialogs stay interactive-only.
    auto *owner=new std::shared_ptr<Dialog>(dialog_);
    auto callback=[](void *data,const char *const *files,int) {
        std::unique_ptr<std::shared_ptr<Dialog>> owner(static_cast<std::shared_ptr<Dialog> *>(data));
        auto state=*owner; std::lock_guard lock(state->mutex);
        if (!files) state->error=SDL_GetError();
        else for (size_t i=0; files[i]; ++i) state->files.emplace_back(files[i]);
        state->done=true;
    };
    if (save) SDL_ShowSaveFileDialog(callback,owner,window_,nullptr,0,dialog_->initial.empty() ? nullptr : dialog_->initial.c_str());
    else SDL_ShowOpenFileDialog(callback,owner,window_,nullptr,0,nullptr,true);
}
void Reader::pollDialog() {
    if (!dialog_) return;
    std::vector<std::string> files; bool save=false; size_t tab=0; std::string failure;
    { std::lock_guard lock(dialog_->mutex); if (!dialog_->done) return; files=dialog_->files; save=dialog_->save; tab=dialog_->tab; failure=dialog_->error; }
    dialog_.reset();
    if (!failure.empty()) throw std::runtime_error(failure);
    if (save) { if (!files.empty()) saveAs(find(tab),utf8Path(files.front())); else saveQueue_.clear(); }
    else { for (const auto &file : files) open(utf8Path(file));openEncoding_="auto"; }
}
void Reader::save(const std::shared_ptr<Tab> &tab,bool as) {
    if (!tab) return;
    sync(tab);
    if (as || tab->document.untitled()) { showDialog(true,tab->id); return; }
    tab->document.save(); tab->diskChanged=false; remember(tab->document.path()); lastError_.clear();
    cyui::notify(tr("Saved"),cyui::Tone::success);
}
void Reader::saveAs(const std::shared_ptr<Tab> &tab,const std::filesystem::path &path) {
    if (!tab) return;
    sync(tab);
    const auto full=std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    for (const auto &other : tabs_) if (other!=tab && sameFile(other->document.path(),full)) throw std::runtime_error(tr("This file is already open in another tab."));
    if (sameFile(tab->document.path(),full)) { save(tab); return; }
    PendingSave pending{tab->id,full,{}};
    if (std::filesystem::exists(full)) { pending.baseline=readTextFileBytes(full); overwrite_=std::move(pending); }
    else commitSave(pending);
}
void Reader::commitSave(const PendingSave &pending) {
    if (auto tab=find(pending.tab)) {
        sync(tab); tab->document.saveAs(pending.path,pending.baseline); tab->diskChanged=false;
        setLanguage(tab); remember(pending.path); lastError_.clear(); cyui::notify(tr("Saved"),cyui::Tone::success);
        if (closeId_==tab->id) resolveClose("discard");
    }
}
void Reader::saveNext() {
    if (saveQueue_.empty() || dialog_ || closeId_ || overwrite_.tab) return;
    const auto id=saveQueue_.front(); saveQueue_.erase(saveQueue_.begin());
    try { save(find(id)); } catch (...) { saveQueue_.clear(); throw; }
}
void Reader::erase(size_t id) {
    core_.remove(id);
    std::erase_if(tabs_,[&](const auto &tab) { return tab->id==id; });
    if (active_==id) { active_=tabs_.empty() ? 0 : tabs_.back()->id; if (auto tab=active()) tab->focus=true; }
}
void Reader::requestClose(size_t id,bool reload) {
    if (dialog_ || closeId_ || overwrite_.tab) return;
    auto tab=find(id); if (!tab) return; sync(tab);
    if (tab->document.dirty()) { closeId_=id; reload_=reload; return; }
    if (reload) { try { tab->document=Document::open(tab->document.path()); install(tab); } catch (const std::exception &exception) { error(exception); } }
    else erase(id);
}
void Reader::requestQuit() {
    std::lock_guard lock(core_.mutex());
    if (dialog_ || overwrite_.tab || closeId_) return;
    for (const auto &tab : tabs_) { sync(tab); if (tab->document.dirty()) { quitting_=true; closeId_=tab->id; return; } }
    quit_=true;
}
void Reader::resolveClose(const std::string &choice) {
    auto tab=find(closeId_); if (!tab) { closeId_=0; return; }
    if (choice=="cancel") { closeId_=0; quitting_=false; reload_=false; ImGui::ClosePopupToLevel(0,true); return; }
    if (choice=="save") { try { save(tab); } catch (const std::exception &exception) { error(exception); return; } if (tab->document.dirty() || dialog_) return; }
    const auto id=closeId_; const bool reload=reload_; closeId_=0; reload_=false; ImGui::ClosePopupToLevel(0,true);
    if (reload) { try { tab->document=Document::open(tab->document.path()); install(tab); } catch (const std::exception &exception) { error(exception); } }
    else erase(id);
    if (quitting_) { quitting_=false; requestQuit(); }
}
void Reader::execute(const std::string &command,const Json &params) {
    std::lock_guard lock(core_.mutex());
    try {
        auto tab=active();
        if (command.starts_with("transform.")) nativeAction("text.transform",{{"operation",command.substr(10)}});
        else if (command=="plugin") nativeAction("plugins.invoke",params);
        else if (command=="format") nativeAction("documents.format",params);
        else if (command=="searchOpen") searchPanel_=true;
        else if (command=="new") newFile();
        else if (command=="open1252") { openEncoding_="windows-1252";showDialog(false); }
        else if (command=="open") { openEncoding_="auto";if (params.contains("path")) open(utf8Path(params.at("path"))); else showDialog(false); }
        else if (command=="save") save(tab);
        else if (command=="saveAs") { if (params.contains("path")) saveAs(tab,utf8Path(params.at("path"))); else save(tab,true); }
        else if (command=="saveAll") { saveQueue_.clear(); for (const auto &item : tabs_) saveQueue_.push_back(item->id); saveNext(); }
        else if (command=="close" && tab) requestClose(tab->id);
        else if (command=="reload" && tab && !tab->document.untitled()) requestClose(tab->id,true);
        else if (command=="quit") requestQuit();
        else if (command=="palette") palette_=true;
        else if (command=="goto") goto_=true;
        else if (command=="find" && tab) tab->editor.OpenFindReplaceWindow();
        else if (command=="nextMatch" && tab) tab->editor.FindNext();
        else if (command=="undo" && tab) { tab->editor.Undo(); sync(tab); }
        else if (command=="redo" && tab) { tab->editor.Redo(); sync(tab); }
        else if (command=="cut" && tab) { tab->editor.Cut(); sync(tab); }
        else if (command=="copy" && tab) tab->editor.Copy();
        else if (command=="paste" && tab) { tab->editor.Paste(); sync(tab); }
        else if (command=="selectAll" && tab) tab->editor.SelectAll();
        else if (command=="nextTab" && !tabs_.empty()) {
            auto it=std::find(tabs_.begin(),tabs_.end(),tab); const auto index=it==tabs_.end() ? 0 : size_t(it-tabs_.begin());
            const auto next=(index+(params.value("reverse",false) ? tabs_.size()-1 : 1))%tabs_.size(); active_=tabs_[next]->id; tabs_[next]->focus=true;
        }
    } catch (const std::exception &exception) { error(exception); }
}
void Reader::shortcuts() {
    if (dialog_ || closeId_ || overwrite_.tab || palette_ || goto_) return;
#ifdef __APPLE__
    const ImGuiKeyChord primary=ImGuiMod_Super;
#else
    const ImGuiKeyChord primary=ImGuiMod_Ctrl;
#endif
    const std::pair<ImGuiKeyChord,const char *> bindings[]={
        {primary|ImGuiKey_N,"new"},{primary|ImGuiKey_O,"open"},{primary|ImGuiKey_S,"save"},
        {primary|ImGuiMod_Shift|ImGuiKey_S,"saveAs"},{primary|ImGuiMod_Alt|ImGuiKey_S,"saveAll"},
        {primary|ImGuiKey_W,"close"},{primary|ImGuiKey_F,"find"},{primary|ImGuiKey_H,"find"},
        {primary|ImGuiMod_Shift|ImGuiKey_F,"searchOpen"},{primary|ImGuiKey_G,"goto"},{primary|ImGuiKey_P,"palette"},
        {primary|ImGuiMod_Shift|ImGuiKey_P,"palette"},{ImGuiKey_F3,"nextMatch"}};
    for (const auto &[chord,action] : bindings) if (ImGui::Shortcut(chord,ImGuiInputFlags_RouteGlobal)) execute(action);
    for (bool reverse : {false,true}) if (ImGui::Shortcut(primary|ImGuiKey_Tab|(reverse ? ImGuiMod_Shift : 0),ImGuiInputFlags_RouteGlobal)) execute("nextTab",{{"reverse",reverse}});
}
void Reader::menus() {
    auto item=[&](const char *label,const char *hint,const char *command,bool enabled=true) { if (ImGui::MenuItem(tr(label),hint,false,enabled)) execute(command); };
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(tr("File"))) {
            item("New file","Ctrl+N","new"); item("Open files...","Ctrl+O","open");
            if (ImGui::BeginMenu(tr("Recent files"))) { const auto recent=recent_; for (const auto &path : recent) if (ImGui::MenuItem(path.c_str())) open(utf8Path(path)); ImGui::EndMenu(); }
            ImGui::Separator(); item("Save","Ctrl+S","save",bool(active())); item("Save as...","Ctrl+Shift+S","saveAs",bool(active()));
            item("Save all","Ctrl+Alt+S","saveAll",!tabs_.empty()); item("Reload from disk",nullptr,"reload",active() && !active()->document.untitled());
            ImGui::Separator(); item("Close file","Ctrl+W","close",bool(active())); item("Exit",nullptr,"quit"); ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Edit"))) {
            auto tab=active(); item("Undo","Ctrl+Z","undo",tab && tab->editor.CanUndo()); item("Redo","Ctrl+Y","redo",tab && tab->editor.CanRedo());
            ImGui::Separator(); item("Cut","Ctrl+X","cut",bool(tab)); item("Copy","Ctrl+C","copy",bool(tab)); item("Paste","Ctrl+V","paste",bool(tab)); item("Select all","Ctrl+A","selectAll",bool(tab));
            if (ImGui::BeginMenu(tr("Text tools"),tab && !tab->editor.IsReadOnlyEnabled())) { for (const auto &[label,id] : std::vector<std::pair<const char*,const char*>>{{"Uppercase (ASCII)","uppercase"},{"Lowercase (ASCII)","lowercase"},{"Trim trailing whitespace","trimTrailing"},{"Sort lines","sortLines"},{"Sort lines descending","sortLinesDescending"},{"Remove duplicate lines","uniqueLines"}}) if (ImGui::MenuItem(tr(label))) execute(std::string("transform.")+id);ImGui::EndMenu(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Search"))) {
            item("Find / replace","Ctrl+F / Ctrl+H","find",bool(active())); item("Find next","F3","nextMatch",bool(active()));
            item("Search open documents","Ctrl+Shift+F","searchOpen");item("Go to line","Ctrl+G","goto",bool(active())); item("Switch file / actions","Ctrl+P","palette"); ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Encoding"))) {
            item("Open Windows-1252 file...",nullptr,"open1252");ImGui::Separator();ImGui::BeginDisabled(!active() || active()->editor.IsReadOnlyEnabled());
            for (const auto &[label,id] : std::vector<std::pair<const char*,const char*>>{{"UTF-8","utf-8"},{"UTF-8 BOM","utf-8-bom"},{"UTF-16 LE BOM","utf-16le"},{"UTF-16 BE BOM","utf-16be"},{"Windows-1252","windows-1252"}}) if (ImGui::MenuItem(label)) execute("format",{{"encoding",id}});
            ImGui::Separator();for (const auto *eol : {"LF","CRLF","CR"}) if (ImGui::MenuItem(eol)) execute("format",{{"lineEnding",eol}});ImGui::EndDisabled();ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("View"))) {
            ImGui::MenuItem(tr("Open files"),nullptr,&sidebar_); ImGui::MenuItem(tr("Word wrap"),nullptr,&wrap_);
            ImGui::MenuItem(tr("Minimap"),nullptr,&minimap_); ImGui::MenuItem(tr("Show whitespace"),nullptr,&whitespace_);
            if (ImGui::MenuItem(tr("Reset layout"))) resetLayout_=true;
            ImGui::Separator(); ImGui::SetNextItemWidth(cyui::dp(160));
            if (ImGui::SliderFloat(tr("Text size"),&textScale_,.8f,2.f,"%.2f")) ImGui::GetStyle().FontScaleMain=textScale_;
            if (ImGui::BeginMenu(tr("Theme"))) { for (auto theme : themes) if (ImGui::MenuItem(theme,nullptr,theme_==theme)) { theme_=theme; applyEditorTheme(theme_); } ImGui::EndMenu(); }
            if (ImGui::BeginMenu(tr("Language"))) { if (ImGui::MenuItem("English",nullptr,locale_=="en")) locale_="en"; if (ImGui::MenuItem("Français",nullptr,locale_=="fr")) locale_="fr"; ImGui::EndMenu(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Syntax"))) {
            for (const auto &[id,language] : pluginLanguages_) if (ImGui::MenuItem(language->name.c_str(),nullptr,active() && active()->language=="plugin:"+id,bool(active()))) { active()->language="plugin:"+id;setLanguage(active()); }
            for (auto language : languages) if (ImGui::MenuItem(std::string(language)=="auto" ? tr("Automatic") : std::string(language)=="plain" ? tr("Plain text") : language,nullptr,active() && active()->language==language, bool(active()))) { active()->language=language; setLanguage(active()); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Extensions"))) { if (ImGui::MenuItem(tr("Extension manager"))) pluginsPanel_=true;const auto state=core_.plugins().describe();for (const auto &plugin : state.at("plugins")) if (ImGui::BeginMenu(plugin.at("name").get_ref<const std::string&>().c_str())) { for (const auto &command : plugin.at("commands")) if (ImGui::MenuItem(command.at("title").get_ref<const std::string&>().c_str(),nullptr,false,active() && !active()->editor.IsReadOnlyEnabled())) execute("plugin",{{"plugin",plugin.at("id")},{"command",command.at("id")}});ImGui::EndMenu(); }ImGui::EndMenu(); }
        if (ImGui::MenuItem("CyToolsCore / MCP")) mcpPanel_=true;
        if (ImGui::MenuItem(tr("Options"))) optionsPanel_=true;
        ImGui::EndMainMenuBar();
    }
}
void Reader::tools() {
    cyui::beginTopBar("##reader-tools",36);
    cyui::textColoured(cyui::palette().accent,"CYCODE READER"); ImGui::SameLine(); cyui::toolbarSeparator();
    if (cyui::iconButton(cyui::icon::add,(std::string(tr("New file"))+" (Ctrl+N)").c_str())) execute("new"); ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::folder,(std::string(tr("Open files..."))+" (Ctrl+O)").c_str())) execute("open"); ImGui::SameLine();
    { cyui::ScopedDisabled disabled(!active());
      if (cyui::iconButton(cyui::icon::save,(std::string(tr("Save"))+" (Ctrl+S)").c_str())) execute("save"); ImGui::SameLine();
      if (cyui::iconButton(cyui::icon::search,(std::string(tr("Find / replace"))+" (Ctrl+F)").c_str())) execute("find"); }
    ImGui::SameLine(); cyui::toolbarSeparator();
    if (auto tab=active()) { cyui::textDim("%s",tab->document.untitled() ? tr("Untitled") : pathUtf8(tab->document.path()).c_str()); }
    cyui::endTopBar();
    cyui::beginStatusBar("##reader-status");
    if (const auto tab=active()) {
        const auto pos=tab->editor.GetMainCursorPosition();
        cyui::statusText(cyui::Tone::neutral,"%s | Ln %zu, Col %zu | %zu %s | %s | %s%s",tab->editor.GetLanguageName().c_str(),pos.line+1,pos.index+1,tab->editor.GetLineCount(),tr("Lines"),tab->document.encoding(),tab->document.lineEnding(),tab->document.dirty() ? " *" : "");
    } else cyui::statusText(cyui::Tone::neutral,"%s",tr("Code editor"));
    cyui::endStatusBar();
}
void Reader::documents() {
    dock_=cyui::dockSpace("ReaderDock");
    if (resetLayout_) {
        ImGui::DockBuilderRemoveNode(dock_); ImGui::DockBuilderAddNode(dock_,ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock_,ImGui::GetMainViewport()->WorkSize);
        editorDock_=dock_;
        const auto left=ImGui::DockBuilderSplitNode(editorDock_,ImGuiDir_Left,.18f,nullptr,&editorDock_);
        ImGui::DockBuilderDockWindow("Open files###reader-files",left);
        ImGui::DockBuilderFinish(dock_);
        for (const auto &tab : tabs_) { ImGui::DockBuilderDockWindow(title(tab).c_str(),editorDock_); tab->detached=false; }
        resetLayout_=false;
    }
    if (sidebar_) {
        if (ImGui::Begin((std::string(tr("Open files"))+"###reader-files").c_str(),&sidebar_)) {
            ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##file-filter",tr("Filter files..."),&filter_);
            for (const auto &tab : tabs_) {
                const auto name=tab->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->document.path().filename());
                if (!contains(name,filter_)) continue;
                ImGui::PushID(static_cast<int>(tab->id));
                if (fileSelectable(name+(tab->document.dirty() ? " *" : ""),tab->document.untitled() ? "new.cpp" : pathUtf8(tab->document.path()),active_==tab->id,theme_)) { active_=tab->id; tab->focus=true; }
                ImGui::PopID();
            }
            if (tabs_.empty()) cyui::textDim("%s",tr("Open or drop code files to begin."));
        }
        ImGui::End();
    }
    if (tabs_.empty()) {
        ImGui::SetNextWindowDockID(editorDock_,ImGuiCond_FirstUseEver);
        if (ImGui::Begin("CyCodeReader###reader-welcome",nullptr,ImGuiWindowFlags_NoCollapse)) {
            cyui::emptyState("CyCodeReader",tr("Open or drop code files to begin."),cyui::icon::code);
            if (cyui::primaryButton(tr("Open files..."))) execute("open"); ImGui::SameLine();
            if (cyui::button(tr("New file"))) execute("new");
            if (!recent_.empty()) { cyui::sectionTitle(tr("Recent files")); const auto recent=recent_; for (const auto &path : recent) if (ImGui::Selectable(path.c_str())) open(utf8Path(path)); }
        }
        ImGui::End();
    }
    std::vector<size_t> closing;
    for (const auto &tab : tabs_) {
        bool visible=true;
        ImGui::SetNextWindowDockID(editorDock_,ImGuiCond_FirstUseEver);
        if (tab->detached) {
            ImGui::SetNextWindowDockID(0,ImGuiCond_Always);
            const auto main=ImGui::GetMainViewport(); ImGui::SetNextWindowPos({main->Pos.x+main->Size.x+cyui::dp(20),main->Pos.y+cyui::dp(60)},ImGuiCond_Always);
            ImGui::SetNextWindowSize(cyui::dp(650,450),ImGuiCond_Always); tab->detached=false;
        }
        if (tab->focus) { ImGui::SetNextWindowFocus(); tab->focus=false; }
        const auto flags=ImGuiWindowFlags_NoCollapse|(tab->document.dirty() ? ImGuiWindowFlags_UnsavedDocument : 0);
        if (ImGui::Begin(title(tab).c_str(),&visible,flags)) {
            documentIcon(tab->document.untitled() ? "new.cpp" : pathUtf8(tab->document.path()),theme_);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) active_=tab->id;
            if (tab->diskChanged) { cyui::callout(cyui::Tone::warning,tr("File changed on disk. Reload it or save a copy.")); if (cyui::button(tr("Reload from disk"))) requestClose(tab->id,true); ImGui::SameLine(); if (cyui::button(tr("Save as..."))) save(tab,true); }
            tab->editor.SetPalette(editorColours(theme_)); tab->editor.SetWordWrapEnabled(wrap_);
            tab->editor.SetShowMiniMapEnabled(minimap_); tab->editor.SetShowWhitespacesEnabled(whitespace_);
            tab->editor.SetFindButtonLabel(tr("Find")); tab->editor.SetFindAllButtonLabel(tr("Find all"));
            tab->editor.SetReplaceButtonLabel(tr("Replace")); tab->editor.SetReplaceAllButtonLabel(tr("Replace all"));
            { cyui::ScopedFont mono(cyui::monoFont()); if (tab->editor.Render(("##reader-text-"+std::to_string(tab->id)).c_str())) tab->version=~size_t(0); }
            sync(tab);
        }
        if (ImGui::BeginPopupContextWindow("##reader-tab-actions",ImGuiPopupFlags_MouseButtonRight|ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem(tr("Move to new window"))) tab->detached=true;
            if (ImGui::MenuItem(tr("Return to editor"))) ImGui::DockBuilderDockWindow(title(tab).c_str(),editorDock_);
            bool readOnly=tab->editor.IsReadOnlyEnabled(); if (ImGui::MenuItem(tr("Read-only"),nullptr,&readOnly)) tab->editor.SetReadOnlyEnabled(readOnly);
            if (ImGui::MenuItem(tr("Close file"))) closing.push_back(tab->id);
            ImGui::EndPopup();
        }
        ImGui::End();
        if (!visible) closing.push_back(tab->id);
    }
    for (auto id : closing) requestClose(id);
}
void Reader::dialogs() {
    if (closeId_) {
        ImGui::OpenPopup(tr("Unsaved changes"));
        if (ImGui::BeginPopupModal(tr("Unsaved changes"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            if (const auto tab=find(closeId_)) ImGui::TextWrapped("%s",title(tab).substr(5,title(tab).find("###")-5).c_str());
            ImGui::TextUnformatted(tr("Save changes before continuing?"));
            if (cyui::primaryButton(tr("Save"))) resolveClose("save"); ImGui::SameLine();
            if (cyui::dangerButton(tr("Discard"))) resolveClose("discard"); ImGui::SameLine();
            if (cyui::button(tr("Cancel"))) resolveClose("cancel");
            if (!closeId_) ImGui::CloseCurrentPopup(); ImGui::EndPopup();
        }
    }
    if (overwrite_.tab) {
        ImGui::OpenPopup(tr("Replace existing file?"));
        if (ImGui::BeginPopupModal(tr("Replace existing file?"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s",pathUtf8(overwrite_.path).c_str());
            if (cyui::dangerButton(tr("Replace"))) { const auto pending=overwrite_; overwrite_={}; try { commitSave(pending); } catch (const std::exception &exception) { error(exception); saveQueue_.clear(); } ImGui::CloseCurrentPopup(); }
            ImGui::SameLine(); if (cyui::button(tr("Cancel"))) { overwrite_={}; saveQueue_.clear(); ImGui::CloseCurrentPopup(); } ImGui::EndPopup();
        }
    }
    if (palette_) ImGui::OpenPopup(tr("Switch file / actions"));
    if (ImGui::BeginPopupModal(tr("Switch file / actions"),&palette_,ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(cyui::dp(540)); ImGui::InputTextWithHint("##palette-query",tr("Type a file name or action..."),&paletteQuery_);
        for (const auto &tab : tabs_) {
            const auto name=tab->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->document.path());
            if (contains(name,paletteQuery_) && ImGui::Selectable(name.c_str())) { active_=tab->id; tab->focus=true; palette_=false; }
        }
        const std::pair<const char *,const char *> actions[]={{"New file","new"},{"Open files...","open"},{"Save all","saveAll"},{"Find / replace","find"},{"Go to line","goto"}};
        ImGui::Separator();
        for (const auto &[name,action] : actions) if (contains(tr(name),paletteQuery_) && ImGui::Selectable(tr(name))) { palette_=false; execute(action); }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || cyui::button(tr("Cancel"))) palette_=false;
        if (!palette_) ImGui::CloseCurrentPopup(); ImGui::EndPopup();
    }
    if (goto_) ImGui::OpenPopup(tr("Go to line"));
    if (ImGui::BeginPopupModal(tr("Go to line"),&goto_,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt(tr("Line"),&line_);
        if (cyui::primaryButton(tr("Go"))) { if (auto tab=active()) tab->editor.SetCursor({static_cast<size_t>(std::max(1,line_)-1),0}); goto_=false; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if (cyui::button(tr("Cancel")) || ImGui::IsKeyPressed(ImGuiKey_Escape)) { goto_=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}
void Reader::frame() {
    try {
        std::lock_guard lock(core_.mutex());
        std::erase_if(tabs_,[&](const auto &tab) { return !core_.documents().contains(tab->id); });
        for (const auto &[id,live] : core_.documents()) if (!find(id)) { auto tab=std::make_shared<Tab>(live);tab->id=id;install(tab);tabs_.push_back(tab);active_=id; }
        if (!find(active_)) active_=tabs_.empty() ? 0 : tabs_.back()->id;
        for (const auto &tab : tabs_) sync(tab);
        pollDialog(); saveNext(); shortcuts(); menus(); tools(); documents(); dialogs();extensionPanels();
        for (const auto &tab : tabs_) { sync(tab);tab->live->readOnly=tab->editor.IsReadOnlyEnabled(); }
        core_.refresh();
        if (ImGui::GetTime()>=pollAt_) {
            pollAt_=ImGui::GetTime()+1.;
            if (auto tab=active(); tab && !tab->document.untitled()) {
                tab->diskChanged=tab->document.changedOnDisk();
                if (tab->diskChanged && !tab->document.dirty()) { tab->document=Document::open(tab->document.path()); install(tab); }
            }
        }
        if (const auto tab=active()) SDL_SetWindowTitle(window_,("CyCodeReader — "+(tab->document.untitled() ? std::string(tr("Untitled")) : pathUtf8(tab->document.path().filename()))+(tab->document.dirty() ? " *" : "")).c_str());
        else SDL_SetWindowTitle(window_,"CyCodeReader");
    } catch (const std::exception &exception) { error(exception); }
    cyui::renderNotifications();
}
Json Reader::state() const {
    std::lock_guard lock(const_cast<Core&>(core_).mutex());
    Json docs=Json::array();
    for (const auto &tab : tabs_) docs.push_back({{"id",tab->id},{"path",pathUtf8(tab->document.path())},
        {"text",tab->document.text()},{"dirty",tab->document.dirty()},{"syntax",tab->editor.GetLanguageName()},
        {"syntaxSpans",tab->syntax.spans()},{"syntaxTruncated",tab->syntax.truncated()},
        {"diskChanged",tab->diskChanged},{"undo",tab->editor.GetUndoIndex()},
        {"hasFind",tab->editor.HasFindString()},{"selection",tab->editor.GetCursorText(0)}});
    return {{"application","CyCodeReader"},{"active",active_},{"documents",docs},{"error",lastError_},
        {"pendingClose",closeId_},{"pendingOverwrite",overwrite_.tab},{"pendingDialog",bool(dialog_)},{"quit",quit_},{"theme",theme_},{"language",locale_},{"trace",trace_}};
}
void Reader::testAction(const Json &action) {
    std::lock_guard lock(core_.mutex());
    if (!options_.automated) throw std::runtime_error("Reader test action requires automated mode");
    const auto command=action.at("command").get<std::string>();
    if (command=="setText") { if (auto tab=active()) { tab->editor.SelectAll(); tab->editor.ReplaceTextPreservingTabs(action.at("text").get<std::string>()); sync(tab); } }
    else if (command=="activate") { active_=action.at("id"); if (auto tab=active()) tab->focus=true; }
    else if (command=="detach") { if (auto tab=active()) tab->detached=true; }
    else if (command=="resolveClose") resolveClose(action.at("choice"));
    else if (command=="resolveOverwrite") { const auto pending=overwrite_; overwrite_={}; ImGui::ClosePopupToLevel(0,true); if (action.value("accept",false)) { try { commitSave(pending); } catch (const std::exception &exception) { error(exception); } } }
    else if (command=="dialogResult") { if (!dialog_) throw std::runtime_error("No pending Reader dialog"); std::lock_guard lock(dialog_->mutex); dialog_->files=action.value("files",std::vector<std::string>{}); dialog_->done=true; }
    else if (command=="clearError") lastError_.clear();
    else if (command=="checkpoint") { auto snapshot=state(); snapshot.erase("trace"); snapshot["name"]=action.at("name"); trace_.push_back(std::move(snapshot)); }
    else execute(command,action);
}
} // namespace cycode::reader
