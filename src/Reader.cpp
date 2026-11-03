#include "Reader.h"
#include "LiveEditor.h"
#include "cycode/EditorLanguage.h"
#include "cycode/EditorTheme.h"
#include "cycode/FileIcons.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <fstream>
#include <cyui/Core.h>
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
constexpr const char *themes[]={"graphite","mid-grey","light-grey","amber-night","violet","navy"};

}
Reader::Reader(Options options,SDL_Window *window) : options_(std::move(options)),core_(options_.installation/"plugins"),window_(window) {
    try { std::ifstream in(options_.installation/"locales/ui.json"); in>>translations_; } catch (...) {}
    if (!options_.automated) try {
        const auto settings=Json::parse(readTextFileBytes(readerDataDirectory(options_.installation)/"reader/preferences.json"));
        updateCheckOnStartup_=settings.value("checkUpdatesAtStartup",false);
        theme_=settings.value("theme",theme_); locale_=settings.value("language",locale_);
        density_=settings.value("density",density_);
        textScale_=std::clamp(settings.value("textScale",textScale_),.8f,2.f);
        if (settings.value("interfaceVersion",0)>=2) {
            wrap_=settings.value("wrap",false);minimap_=settings.value("minimap",true);
            sidebar_=settings.value("sidebar",false);whitespace_=settings.value("whitespace",false);
        }
        if (settings.contains("recent")) recent_=settings.at("recent").get<std::vector<std::string>>();
        if (settings.contains("pluginOptions")) for (const auto &[id,value] : settings.at("pluginOptions").items()) try { core_.plugins().configure(id,value); } catch (...) {}
        if (recent_.size()>12) recent_.resize(12);
    } catch (...) {}
    if (!options_.theme.empty()) theme_=options_.theme;
    if (!options_.language.empty()) locale_=options_.language;
    if (std::find(std::begin(themes),std::end(themes),theme_)==std::end(themes)) theme_="graphite";
    if (locale_!="en" && locale_!="fr") locale_="en";
    installTranslations();cyui::setLocale(locale_.c_str());
    cyui::Config config; config.themeKey=theme_.c_str(); config.squareWindows=true;
    config.density=density_=="comfortable" ? cyui::Density::comfortable : cyui::Density::compact;
    config.dpiScale=window_ ? SDL_GetWindowDisplayScale(window_) : 1.f; cyui::apply(config);
    applyEditorTheme(theme_); cyui::loadFonts(); ImGui::GetStyle().FontScaleMain=textScale_;
    preparePluginLanguages();
    prepareFileProviders();
    for (const auto &path : options_.files) open(path);
    if (!options_.automated) startSdk();
}
Reader::~Reader() {
    if (comparison_.pending.valid()) comparison_.pending.wait();
    if (history_.pending.valid()) history_.pending.wait();
    if (history_.diff.pending.valid()) history_.diff.pending.wait();
    for (auto &provider : fileProviders_) if (provider.pending.valid()) provider.pending.wait();
    updateCancel_=true;
    if (releaseCheck_.valid()) releaseCheck_.wait();
    if (updateDownload_.valid()) updateDownload_.wait();
    bridge_.reset();
    if (sdk_.valid()) { if (sdk_.wait_for(std::chrono::seconds(2))!=std::future_status::ready) sdkStop_=true; sdk_.wait(); }
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
    for (const auto &[id,language] : pluginSyntax_) cyui::unregisterCodeLanguage(id);
#endif
    if (options_.automated) return;
    try {
        const auto path=readerDataDirectory(options_.installation)/"reader/preferences.json";
        std::filesystem::create_directories(path.parent_path());
        std::optional<std::string> baseline;
        if (std::filesystem::exists(path)) baseline=readTextFileBytes(path);
        const auto pluginState=core_.plugins().describe();Json pluginOptions=Json::object();for (const auto &plugin : pluginState.at("plugins")) pluginOptions[plugin.at("id").get<std::string>()]=plugin.at("options");
        writeTextFileAtomic(path,Json{{"checkUpdatesAtStartup",updateCheckOnStartup_},{"pluginOptions",pluginOptions},{"density",density_},{"theme",theme_},{"language",locale_},{"textScale",textScale_},
            {"interfaceVersion",2},{"sidebar",sidebar_},{"whitespace",whitespace_},{"wrap",wrap_},{"minimap",minimap_},{"recent",recent_}}.dump(2),baseline);
    } catch (...) {}
}
const char *Reader::tr(const char *key) const {
    const auto it=translations_.find(key);
    return it!=translations_.end() && it->contains(locale_) ? it->at(locale_).get_ref<const std::string &>().c_str() : key;
}
void Reader::error(const std::exception &exception) { lastError_=exception.what(); serviceLogs_.append(cyui::LogLevel::error,lastError_,"Reader");cyui::notify(lastError_.c_str(),cyui::Tone::error); }
void Reader::applyAppearance() {
    cyui::setLocale(locale_.c_str());
    cyui::Config config;
    config.themeKey=theme_=="graphite" ? "navy" : theme_.c_str();config.squareWindows=true;
    config.density=density_=="comfortable" ? cyui::Density::comfortable : cyui::Density::compact;
    config.dpiScale=cyui::dpi();cyui::apply(config);applyEditorTheme(theme_);
    ImGui::GetStyle().FontScaleMain=textScale_;
}
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
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
    const auto &mode=tab->language;
    tab->syntaxPath=tab->document.untitled() ? "code.cpp" : pathUtf8(tab->document.path());
    const bool unreal=mode=="C++ / Unreal" || mode=="unreal" ||
        (mode=="auto" && !tab->document.untitled() && unrealFile(tab->document.path()));
    auto selected=mode=="auto" ? cyui::codeLanguageForPath(tab->syntaxPath) :
        cyui::codeLanguageFromName(mode.starts_with("plugin:") ? mode.substr(7) : mode);
    if (mode=="TypeScript / TSX") selected=cyui::CodeLanguage::tsx;
    if (unreal && (selected==cyui::CodeLanguage::cpp || selected==cyui::CodeLanguage::c || mode=="C++ / Unreal")) selected=cyui::CodeLanguage::unreal;
    const auto info=cyui::codeLanguageInfo(selected);
    if (mode!="auto") tab->syntaxPath=info.extensions.empty() ? "code.txt" : "code."+info.extensions.front();
    if (selected==cyui::CodeLanguage::unreal) tab->syntaxPath="code.cpp";
    // Preserve the context-aware Unreal/C# tokenizer and existing incremental parsers.
    const bool rich=selected==cyui::CodeLanguage::c || selected==cyui::CodeLanguage::cpp ||
        selected==cyui::CodeLanguage::unreal || selected==cyui::CodeLanguage::csharp ||
        selected==cyui::CodeLanguage::rust || selected==cyui::CodeLanguage::java ||
        selected==cyui::CodeLanguage::javascript || selected==cyui::CodeLanguage::typescript ||
        selected==cyui::CodeLanguage::tsx || selected==cyui::CodeLanguage::html || selected==cyui::CodeLanguage::css;
    tab->codeEditor.setLanguage(selected);tab->codeEditor.setSyntaxEnabled(!rich);
    if (rich) tab->editor.SetLanguage(editorLanguage(tab->syntaxPath,unreal));
    // A manually selected plain/plugin mode must not keep an old Tree-sitter overlay.
    if (!rich) tab->syntaxPath="code.txt";
    tab->syntax.update(tab->editor,tab->syntaxPath,tab->document.text());
#else
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
#endif
}
void Reader::install(const std::shared_ptr<Tab> &tab) {
    configureCodeEditor(tab->codeEditor);
    tab->editor.SetFindReplaceEnabled(false);
    const auto id=tab->id;
    tab->editor.SetTextContextMenuCallback([this,id](TextEditor::PopupData &data) { codeContext(id,data,false); });
    tab->editor.SetLineNumberContextMenuCallback([this,id](TextEditor::PopupData &data) { codeContext(id,data,true); });
    tab->editor.SetInsertSpacesOnTabs(false); tab->codeEditor.setText(tab->document.text());
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
void Reader::showDialog(bool save,size_t tab,bool compare) {
    if (dialog_) return;
    dialog_=std::make_shared<Dialog>(); dialog_->save=save; dialog_->tab=tab;dialog_->compare=compare;
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
    else SDL_ShowOpenFileDialog(callback,owner,window_,nullptr,0,nullptr,!compare);
}
void Reader::pollDialog() {
    if (!dialog_) return;
    std::vector<std::string> files; bool save=false,compare=false; size_t tab=0; std::string failure;
    { std::lock_guard lock(dialog_->mutex); if (!dialog_->done) return; files=dialog_->files; save=dialog_->save;compare=dialog_->compare; tab=dialog_->tab; failure=dialog_->error; }
    dialog_.reset();
    if (!failure.empty()) throw std::runtime_error(failure);
    if (compare) { if (!files.empty()) compareFile(tab,utf8Path(files.front())); }
    else if (save) { if (!files.empty()) saveAs(find(tab),utf8Path(files.front())); else saveQueue_.clear(); }
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
    if (choice=="cancel") { updateOnExit_.reset();closeId_=0; quitting_=false; reload_=false; ImGui::ClosePopupToLevel(0,true); return; }
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
        if (command.starts_with("history.")) historyAction(command,params);
        else if (command.starts_with("compare.")) compareAction(command,params);
        else if (command.starts_with("transform.")) nativeAction("text.transform",{{"operation",command.substr(10)}});
        else if (command=="plugin") nativeAction("plugins.invoke",params);
        else if (command=="view.toggle") {
            const auto option=params.at("option").get<std::string>();
            if (option=="sidebar") sidebar_=!sidebar_;else if (option=="wrap") wrap_=!wrap_;
            else if (option=="minimap") minimap_=!minimap_;else if (option=="whitespace") whitespace_=!whitespace_;
        }
        else if (command=="view.resetLayout") { resetLayout_=true;sidebar_=false;minimap_=true;wrap_=false;whitespace_=false; }
        else if (command=="view.scale") { textScale_=std::clamp(params.value("value",textScale_+params.value("delta",0.f)),.8f,2.f);ImGui::GetStyle().FontScaleMain=textScale_; }
        else if (command=="view.locale") { const auto value=params.at("locale").get<std::string>();if (value=="en" || value=="fr") { locale_=value;cyui::setLocale(locale_.c_str()); } }
        else if (command=="view.theme") { const auto value=params.at("theme").get<std::string>();if (std::find(std::begin(themes),std::end(themes),value)!=std::end(themes)) { theme_=value;applyAppearance(); } }
        else if (command=="syntax" && tab) { tab->language=params.at("language");setLanguage(tab); }
        else if (command=="format") nativeAction("documents.format",params);
        else if (command=="searchOpen") openFind(false,true);
        else if (command=="plugins") pluginsPanel_=true;
        else if (command=="mcp") mcpPanel_=true;
        else if (command=="options") optionsPanel_=true;
        else if (command=="updates") { updatePanel_=true; }
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
        else if (command=="find" && tab) openFind();
        else if (command=="replace" && tab) openFind(true);
        else if (command=="nextMatch" && tab) findMatch();
        else if (command=="previousMatch" && tab) findMatch(true);
        else if (command=="replaceOne") replaceMatches(false);
        else if (command=="replaceAll") replaceMatches(true);
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
    // ImGui >= 1.92 normalizes physical Cmd to Ctrl with macOS behaviors.
    const ImGuiKeyChord primary=ImGuiMod_Ctrl;
    if (ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)) return;
    const std::pair<ImGuiKeyChord,const char *> bindings[]={
        {primary|ImGuiKey_N,"new"},{primary|ImGuiKey_O,"open"},{primary|ImGuiKey_S,"save"},
        {primary|ImGuiMod_Shift|ImGuiKey_S,"saveAs"},{primary|ImGuiMod_Alt|ImGuiKey_S,"saveAll"},
        {primary|ImGuiKey_W,"close"},{primary|ImGuiKey_F,"find"},{primary|ImGuiKey_H,"replace"},
        {primary|ImGuiMod_Shift|ImGuiKey_F,"searchOpen"},{primary|ImGuiKey_G,"goto"},{primary|ImGuiKey_P,"palette"},
        {primary|ImGuiMod_Shift|ImGuiKey_P,"palette"},{ImGuiKey_F3,"nextMatch"},{ImGuiMod_Shift|ImGuiKey_F3,"previousMatch"}};
    for (const auto &[chord,action] : bindings) {
        ImGuiInputFlags flags=ImGuiInputFlags_RouteGlobal;
        // Ctrl+Shift+F belongs to Reader; the native widget also binds it to Find All.
        if (std::string_view(action)=="searchOpen") flags|=ImGuiInputFlags_RouteOverActive;
        if (ImGui::Shortcut(chord,flags)) {
            if (chord==(primary|ImGuiKey_F) && routeFilterFind()) continue;
            execute(action);
        }
    }
    for (bool reverse : {false,true}) if (ImGui::Shortcut(primary|ImGuiKey_Tab|(reverse ? ImGuiMod_Shift : 0),ImGuiInputFlags_RouteGlobal)) execute("nextTab",{{"reverse",reverse}});
}
void Reader::tools() {
    cyui::beginTopBar("##reader-tools",cyui::dp(36));
    if (options_.logo) { ImGui::Image(ImTextureRef(options_.logo),cyui::dp(24,24));ImGui::SameLine(); }
    cyui::toolbarSeparator();
    const auto tab=active();
    float right=ImGui::GetWindowContentRegionMax().x;
    auto tool=[&](const char *icon,const char *label,const char *key,const char *command,bool enabled=true,bool checked=false,Json params=nlohmann::json::object()) {
        const float width=ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.x;
        if (ImGui::GetCursorPosX()+width>right) return;
        cyui::ScopedDisabled disabled(!enabled);
        const auto tip=std::string(tr(label))+(key[0] ? std::string(" (")+key+")" : "");
        if (cyui::iconButton(icon,tip.c_str(),checked)) execute(command,params);
        ImGui::SameLine();
    };
    tool(cyui::icon::add,"New file","Ctrl+N","new");tool(cyui::icon::folder,"Open files...","Ctrl+O","open");
    tool(cyui::icon::save,"Save","Ctrl+S","save",bool(tab));tool(cyui::icon::copy,"Save all","Ctrl+Alt+S","saveAll",bool(tab));
    tool(cyui::icon::close,"Close file","Ctrl+W","close",bool(tab));
    cyui::toolbarSeparator();
    tool(cyui::icon::undo,"Undo","Ctrl+Z","undo",tab && tab->editor.CanUndo());tool(cyui::icon::redo,"Redo","Ctrl+Y","redo",tab && tab->editor.CanRedo());
    tool(cyui::icon::cut,"Cut","Ctrl+X","cut",tab && !tab->editor.IsReadOnlyEnabled());tool(cyui::icon::copy,"Copy","Ctrl+C","copy",bool(tab));
    tool(cyui::icon::paste,"Paste","Ctrl+V","paste",tab && !tab->editor.IsReadOnlyEnabled());
    cyui::toolbarSeparator();
    tool(cyui::icon::search,"Find","Ctrl+F","find",bool(tab));tool(cyui::icon::edit,"Replace","Ctrl+H","replace",bool(tab));
    tool(cyui::icon::filter,"Search open documents","Ctrl+Shift+F","searchOpen");
    tool(cyui::icon::code,"Compare files","","compare.open",bool(tab));
    for (size_t p=0;p<fileProviders_.size();++p) if (!fileProviders_[p].history.empty())
        tool(cyui::icon::clock,"File history","","history.open",providerReady(p,fileProviders_[p].history.at("file")),false,{{"provider",p}});
    tool(cyui::icon::library,"Minimap","","view.toggle",true,minimap_,{{"option","minimap"}});
    tool(cyui::icon::code,"Show whitespace","","view.toggle",true,whitespace_,{{"option","whitespace"}});
    if (tab && ImGui::GetContentRegionAvail().x>cyui::dp(100.f)) {
        const auto path=tab->document.untitled() ? std::string(tr("Untitled")) : pathUtf8(tab->document.path());
        cyui::clippedText(ImGui::GetWindowDrawList(),ImGui::GetCursorScreenPos(),ImGui::GetContentRegionAvail().x,cyui::palette().textDim,path.c_str());
    }
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
        if (sidebar_) { const auto left=ImGui::DockBuilderSplitNode(editorDock_,ImGuiDir_Left,.18f,nullptr,&editorDock_);ImGui::DockBuilderDockWindow("Open files###reader-files",left); }
        ImGui::DockBuilderFinish(dock_);
        for (const auto &tab : tabs_) { ImGui::DockBuilderDockWindow(title(tab).c_str(),editorDock_); tab->detached=false; }
        resetLayout_=false;
    }
    if (sidebar_) {
        if (ImGui::Begin((std::string(tr("Open files"))+"###reader-files").c_str(),&sidebar_)) {
            registerFilterOwner(filesFilter_);
            cyui::FilterOptions field;field.hint="Filter files...";
            cyui::filterBox("##file-filter",filesFilter_,field);
            size_t visibleFiles=0;
            for (const auto &tab : tabs_) {
                const auto name=tab->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->document.path().filename());
                const auto keywords=tab->document.untitled() ? std::string("Untitled ")+tr("Untitled") : pathUtf8(tab->document.path());
                if (!filesFilter_.matches(name,keywords)) continue;
                ++visibleFiles;
                ImGui::PushID(static_cast<int>(tab->id));
                if (fileSelectable(name+(tab->document.dirty() ? " *" : ""),tab->document.untitled() ? "new.cpp" : pathUtf8(tab->document.path()),active_==tab->id,theme_)) { active_=tab->id; tab->focus=true; }
                ImGui::PopID();
            }
            if (tabs_.empty()) cyui::textDim("%s",tr("Open or drop code files to begin."));
            else if (!visibleFiles) cyui::textDim("%s",tr("No matching files"));
        }
        ImGui::End();
    }
    if (tabs_.empty()) {
        ImGui::SetNextWindowDockID(editorDock_,ImGuiCond_FirstUseEver);
        if (ImGui::Begin("CyCodeReader###reader-welcome",nullptr,ImGuiWindowFlags_NoCollapse)) {
            if (options_.logo) {
                const auto x=ImGui::GetCursorPosX(),size=cyui::dp(96.f);
                ImGui::SetCursorPosX(x+std::max(0.f,(ImGui::GetContentRegionAvail().x-size)*.5f));
                ImGui::Image(ImTextureRef(options_.logo),{size,size});ImGui::SetCursorPosX(x);
            }
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
            tab->editor.SetWordWrapEnabled(wrap_);
            tab->editor.SetShowMiniMapEnabled(minimap_); tab->editor.SetShowWhitespacesEnabled(whitespace_);
            tab->editor.SetFindButtonLabel(tr("Find")); tab->editor.SetFindAllButtonLabel(tr("Find all"));
            tab->editor.SetReplaceButtonLabel(tr("Replace")); tab->editor.SetReplaceAllButtonLabel(tr("Replace all"));
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
            try { tab->codeEditor.refreshSyntax(); }
            catch (const std::exception &e) {
                // A failed extension must leave the live editor usable and report once.
                tab->codeEditor.setSyntaxEnabled(false);tab->editor.SetLanguage(nullptr);error(e);
            }
#endif
            updateSearchMarkers(tab);
            if (tab->codeEditor.draw(("##reader-text-"+std::to_string(tab->id)).c_str())) tab->version=~size_t(0);
            sync(tab);
            if (!contextCommand_.empty()) { auto command=std::move(contextCommand_);contextCommand_.clear();execute(command,contextParams_); }
        }
        if (ImGui::BeginPopupContextWindow("##reader-tab-actions",ImGuiPopupFlags_MouseButtonRight|ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem(tr("Select for comparison"))) execute("compare.select",{{"id",tab->id}});
            if (ImGui::MenuItem(tr("Compare with selected tab"),nullptr,false,find(compareSelected_) && compareSelected_!=tab->id)) execute("compare.selected",{{"id",tab->id}});
            if (ImGui::MenuItem(tr("Compare with saved version"),nullptr,false,!tab->document.untitled())) execute("compare.saved",{{"id",tab->id}});
            for (size_t p=0;p<fileProviders_.size();++p) if (tab->id==active_) for (const auto &action : fileProviders_[p].actions) {
                const auto command=action.at("command").get<std::string>();
                if (ImGui::MenuItem(tr(action.at("title").get_ref<const std::string&>().c_str()),nullptr,false,providerReady(p,command))) execute("compare.provider",{{"provider",p},{"command",command}});
            }
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
    // The application maps stable IDs to its existing actions; CyUI owns matching,
    // scrolling, keyboard focus, arrows, Enter and Escape.
    std::vector<cyui::Command> commands;
    std::map<cyui::ItemId,size_t> documentCommands;
    std::map<cyui::ItemId,std::string> actionsById;
    if (palette_) {
        for (const auto &tab : tabs_) {
            const auto name=tab->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->document.path());
            const auto id=uiId("document/"+std::to_string(tab->id));
            commands.push_back({id,name,"",tr("Documents")});documentCommands.emplace(id,tab->id);
        }
        const char *actions[][3]={{"New file","new","Ctrl+N"},{"Open files...","open","Ctrl+O"},
            {"Save all","saveAll","Ctrl+Alt+S"},{"Find / replace","find","Ctrl+F"},{"Go to line","goto","Ctrl+G"},{"Search open documents","searchOpen","Ctrl+Shift+F"},
            {"Extensions","plugins",""},{"Compare files","compare.open",""},{"Compare with saved version","compare.saved",""},{"Options","options",""},{"Updates","updates",""},{"CyToolsCore / MCP","mcp",""}};
        for (const auto &action : actions) {
            const auto id=uiId(std::string("action/")+action[1]);
            const std::string command=action[1];
            const bool enabled=command=="compare.saved" ? active() && !active()->document.untitled() : active() || command=="new" || command=="open" || command=="updates" || command=="compare.open";
            commands.push_back({id,tr(action[0]),action[2],tr("Actions"),enabled});
            actionsById.emplace(id,action[1]);
        }
        for (size_t p=0;p<fileProviders_.size();++p) for (const auto &action : fileProviders_[p].actions) {
            const auto command=action.at("command").get<std::string>();const auto id=uiId("provider/"+fileProviders_[p].plugin+"/"+command);
            commands.push_back({id,tr(action.at("title").get_ref<const std::string&>().c_str()),"",fileProviders_[p].menu,providerReady(p,command) && !comparison_.pending.valid()});
        }
    }
    const auto request=cyui::commandPalette((std::string(tr("Switch file / actions"))+"###reader-command-palette").c_str(),palette_,commandState_,commands,cyui::dp(620,380));
    if (documentCommands.contains(request)) { if (auto tab=find(documentCommands.at(request))) { active_=tab->id;tab->focus=true; } }
    else if (actionsById.contains(request)) execute(actionsById.at(request));
    else for (size_t p=0;p<fileProviders_.size();++p) for (const auto &action : fileProviders_[p].actions) {
        const auto command=action.at("command").get<std::string>();
        if (request==uiId("provider/"+fileProviders_[p].plugin+"/"+command)) execute("compare.provider",{{"provider",p},{"command",command}});
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
        core_.refresh();pollFileProviders();
        pollDialog(); saveNext(); shortcuts(); menus(); tools(); documents(); dialogs();extensionPanels();searchWindows();historyPanel();updatePanel();comparisonPanel();
        for (const auto &tab : tabs_) { sync(tab);tab->live->readOnly=tab->editor.IsReadOnlyEnabled(); }
        core_.refresh();
        if (ImGui::GetTime()>=pollAt_) {
            pollAt_=ImGui::GetTime()+1.;
            if (auto tab=active(); tab && !tab->document.untitled()) {
                tab->diskChanged=tab->document.changedOnDisk();
                if (tab->diskChanged && !tab->document.dirty()) { tab->document=Document::open(tab->document.path()); install(tab); }
            }
        }
        if (const auto tab=active(); tab && window_) SDL_SetWindowTitle(window_,("CyCodeReader — "+(tab->document.untitled() ? std::string(tr("Untitled")) : pathUtf8(tab->document.path().filename()))+(tab->document.dirty() ? " *" : "")).c_str());
        else if (window_) SDL_SetWindowTitle(window_,"CyCodeReader");
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
