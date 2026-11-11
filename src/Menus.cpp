#include "Reader.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <imgui_internal.h>

namespace cycode::reader {
void Reader::installTranslations() {
    for (const auto *locale : {"en","fr"}) {
        cyui::TranslationCatalog catalog;
        for (const auto &[key,entry] : translations_.items()) if (entry.is_object() && entry.contains(locale) && entry.at(locale).is_string()) catalog.emplace(key,entry.at(locale).get<std::string>());
        cyui::registerTranslations(locale,catalog);
    }
}
MenuModel Reader::menuModel(const std::string &menu) const {
    MenuModel model;
    auto &rows=model.items;
    const auto tab=active();const bool editable=tab && !tab->readOnly();
    const bool ordinaryEditable=editable && !tab->paged;
    auto add=[&](auto &into,const std::string &label,const std::string &command,Json params=Json::object(),
                 const std::string &shortcut="",bool enabled=true,bool checked=false,const std::string &keywords="") {
        return model.add(into,label,command,std::move(params),shortcut,enabled,checked,keywords);
    };
    auto separator=[&] { rows.push_back({}); };
    if (menu=="File") {
        add(rows,"New file","new",{},"Ctrl+N");add(rows,"Open files...","open",{},"Ctrl+O");
        cyui::FilterMenuItem recent;recent.label="Recent files";
        for (const auto &path : recent_) add(recent.children,path,"open",{{"path",path}},"",true,false,path);
        if (!recent.children.empty()) rows.push_back(std::move(recent));
        separator();add(rows,"Save","save",{},"Ctrl+S",bool(tab));add(rows,"Save as...","saveAs",{},"Ctrl+Shift+S",bool(tab));
        add(rows,"Save all","saveAll",{},"Ctrl+Alt+S",!tabs_.empty());add(rows,"Reload from disk","reload",{},"",tab && !tab->untitled());
        separator();add(rows,"Close file","close",{},"Ctrl+W",bool(tab));add(rows,"Exit","quit");
    } else if (menu=="Edit") {
        add(rows,"Undo","undo",{},"Ctrl+Z",tab && tab->canUndo());add(rows,"Redo","redo",{},"Ctrl+Y",tab && tab->canRedo());separator();
        add(rows,"Cut","cut",{},"Ctrl+X",editable);add(rows,"Copy","copy",{},"Ctrl+C",bool(tab));
        add(rows,"Paste","paste",{},"Ctrl+V",editable);add(rows,"Select all","selectAll",{},"Ctrl+A",bool(tab));
        cyui::FilterMenuItem tools;tools.label="Text tools";tools.enabled=ordinaryEditable;
        for (const auto &[label,id] : std::vector<std::pair<const char*,const char*>>{{"Uppercase (ASCII)","uppercase"},{"Lowercase (ASCII)","lowercase"},{"Trim trailing whitespace","trimTrailing"},{"Sort lines","sortLines"},{"Sort lines descending","sortLinesDescending"},{"Remove duplicate lines","uniqueLines"}})
            add(tools.children,label,std::string("transform.")+id,{},"",ordinaryEditable);
        rows.push_back(std::move(tools));
    } else if (menu=="Search") {
        add(rows,"Find","find",{},"Ctrl+F",bool(tab));add(rows,"Replace","replace",{},"Ctrl+H",bool(tab));add(rows,"Find next","nextMatch",{},"F3",bool(tab));add(rows,"Find previous","previousMatch",{},"Shift+F3",bool(tab));
        add(rows,"Search open documents","searchOpen",{},"Ctrl+Shift+F");add(rows,"Go to line","goto",{},"Ctrl+G",bool(tab));add(rows,"Switch file / actions","palette",{},"Ctrl+P");
    } else if (menu=="Compare") {
        add(rows,"Compare files","compare.open");
        add(rows,"Select for comparison","compare.select",{},"",bool(tab));
        add(rows,"Compare with selected tab","compare.selected",{},"",tab && find(compareSelected_) && tab->id!=compareSelected_);
        add(rows,"Compare with saved version","compare.saved",{},"",tab && !tab->untitled());
        add(rows,"Compare with file...","compare.file",{},"",bool(tab));
    } else if (menu=="Encoding") {
        add(rows,"Open Windows-1252 file...","open1252");separator();
        for (const auto &[label,id] : std::vector<std::pair<const char*,const char*>>{{"UTF-8","utf-8"},{"UTF-8 BOM","utf-8-bom"},{"UTF-16 LE BOM","utf-16le"},{"UTF-16 BE BOM","utf-16be"},{"Windows-1252","windows-1252"}})
            add(rows,label,"format",{{"encoding",id}},"",ordinaryEditable,tab && std::string(tab->document.encoding())==label);
        separator();for (const auto *eol : {"LF","CRLF","CR"}) add(rows,eol,"format",{{"lineEnding",eol}},"",ordinaryEditable,tab && std::string(tab->document.lineEnding())==eol);
    } else if (menu=="View") {
        for (const auto &[label,key,value] : std::vector<std::tuple<const char*,const char*,bool>>{{"Open files","sidebar",sidebar_},{"Word wrap","wrap",wrap_},{"Minimap","minimap",minimap_},{"Show whitespace","whitespace",whitespace_}})
            add(rows,label,"view.toggle",{{"option",key}},"",std::string_view(key)=="sidebar" || !tab || !tab->paged,value && (std::string_view(key)=="sidebar" || !tab || !tab->paged));
        add(rows,"Reset layout","view.resetLayout");separator();
        cyui::FilterMenuItem size;size.label="Text size";
        add(size.children,"Increase text size","view.scale",{{"delta",.1f}},"",textScale_<2.f,false,"zoom");
        add(size.children,"Decrease text size","view.scale",{{"delta",-.1f}},"",textScale_>.8f,false,"zoom");
        add(size.children,"Reset text size","view.scale",{{"value",1.15f}},"",true,false,"zoom");rows.push_back(std::move(size));
        cyui::FilterMenuItem themes;themes.label="Theme";
        for (const auto *key : {"graphite","mid-grey","light-grey","amber-night","violet","navy"}) add(themes.children,key,"view.theme",{{"theme",key}},"",true,theme_==key);
        rows.push_back(std::move(themes));cyui::FilterMenuItem locale;locale.label="Language";
        add(locale.children,"English","view.locale",{{"locale","en"}},"",true,locale_=="en");add(locale.children,"Français","view.locale",{{"locale","fr"}},"",true,locale_=="fr");rows.push_back(std::move(locale));
    } else if (menu=="Syntax") {
        add(rows,"Automatic","syntax",{{"language","auto"}},"",bool(tab),tab && tab->language=="auto");
        add(rows,"Plain text","syntax",{{"language","plain"}},"",bool(tab),tab && tab->language=="plain");separator();
        auto catalogue=cyui::codeLanguages();std::sort(catalogue.begin(),catalogue.end(),[](const auto &a,const auto &b) { return a.name<b.name; });
        auto initial=[](const std::string &name) {
            const auto folded=cyui::foldFilterText(name);
            return !folded.empty() && folded.front()>='a' && folded.front()<='z' ? char(folded.front()-'a'+'A') : '#';
        };
        for (char letter='A';letter<='Z'+1;++letter) {
            const char key=letter=='Z'+1 ? '#' : letter;
            cyui::FilterMenuItem group;group.label=key=='#' ? "Other syntaxes" : std::string(1,key);
            for (const auto &language : catalogue) if (language.language!=cyui::CodeLanguage::plain && initial(language.name)==key) {
                std::string keywords=language.id;for (const auto &ext : language.extensions) keywords+=" ."+ext;
                for (const auto &file : language.filenames) keywords+=" "+file;
                add(group.children,language.name,"syntax",{{"language",language.id}},"",bool(tab),tab && tab->codeEditor.language()==language.language,keywords);
            }
            if (!group.children.empty()) rows.push_back(std::move(group));
        }
    } else if (menu=="Help") {
        add(rows,"Documentation","help",{},"F1");
        add(rows,"Context help","help.context",{},"Shift+F1");
        add(rows,"Illustrated tour","help.tour");
        add(rows,"Keyboard shortcuts","help",{{"page","shortcuts"}});
        add(rows,"Extension documentation","help",{{"page","plugins"}});
        add(rows,"MCP documentation","help",{{"page","mcp"}});
    } else if (menu=="Extensions") {
        add(rows,"Extension manager","plugins");separator();
        const auto state=const_cast<Core&>(core_).plugins().describe();
        for (const auto &plugin : state.at("plugins")) {
            cyui::FilterMenuItem group;group.label=plugin.at("name");group.keywords=plugin.at("id");
            for (const auto &command : plugin.at("commands")) if (command.value("kind","")!="syntax" && command.value("kind","")!="service")
                add(group.children,command.at("title"),"plugin",{{"plugin",plugin.at("id")},{"command",command.at("id")}},"",editable,false,command.at("id"));
            if (!group.children.empty()) rows.push_back(std::move(group));
        }
    }
    return model;
}
MenuModel Reader::providerMenuModel(size_t index) const {
    MenuModel model;if (index>=fileProviders_.size()) return model;
    const auto &provider=fileProviders_[index];
    for (const auto &action : provider.actions) {
        const auto command=action.at("command").get<std::string>();
        model.add(model.items,action.at("title"),"compare.provider",{{"provider",index},{"command",command}},"",providerReady(index,command) && !comparison_.pending.valid());
    }
    if (!provider.history.empty()) {
        model.add(model.items,"File history","history.open",{{"provider",index}},"",providerReady(index,provider.history.at("file")));
        model.add(model.items,"Selection history","history.selection",{{"provider",index}},"",providerReady(index,provider.history.at("selection")));
    }
    model.add(model.items,"Refresh repository status","compare.detect",{},"",!provider.pending.valid());return model;
}
void Reader::runMenuAction(const MenuModel &model,cyui::ItemId id) {
    const auto at=model.actions.find(id);
    if (at!=model.actions.end() && at->second.enabled) execute(at->second.command,at->second.params);
}
void Reader::menus() {
    if (!ImGui::BeginMainMenuBar()) return;
    for (const auto *menu : {"File","Edit","Search","Compare","Encoding","View","Syntax","Extensions","Help"}) {
        if (ImGui::BeginMenu(cyui::trLabel(menu).c_str())) {
            const auto model=menuModel(menu);
            auto &filter=menuFilters_[menu];
            const auto request=cyui::filterMenu("##reader-menu",filter,model.items,{300,true,true});
            if (std::string_view(menu)=="View" && filter.matches("Text size","zoom")) {
                ImGui::Separator();ImGui::SetNextItemWidth(cyui::dp(200));
                if (ImGui::SliderFloat(cyui::trLabel("Text size").c_str(),&textScale_,.8f,2.f,"%.2f")) ImGui::GetStyle().FontScaleMain=textScale_;
            }
            if (request) runMenuAction(model,request);
            ImGui::EndMenu();
        }
    }
    for (size_t p=0;p<fileProviders_.size();++p) if (ImGui::BeginMenu(cyui::trLabel(fileProviders_[p].menu.c_str()).c_str())) {
        const auto &provider=fileProviders_[p];
        const auto model=providerMenuModel(p);auto &filter=menuFilters_[provider.plugin];
        const auto request=cyui::filterMenu("##reader-provider-menu",filter,model.items,{300,true,true});if (request) runMenuAction(model,request);
        if (provider.pending.valid()) ImGui::TextDisabled("%s",tr("Detecting repository..."));
        else if (provider.context.contains("message")) ImGui::TextWrapped("%s",tr(provider.context.at("message").get_ref<const std::string&>().c_str()));
        else if (providerReady(p)) {
            ImGui::Separator();ImGui::TextDisabled("%s",provider.context.value("root","").c_str());
            ImGui::TextDisabled("%s",provider.context.value("detached",false) ? tr("Detached HEAD") : provider.context.value("branch","").c_str());
            if (!provider.context.value("tracked",true)) ImGui::TextDisabled("%s",tr("Untracked file; missing Git versions are empty"));
            if (provider.context.value("conflicted",false)) ImGui::TextDisabled("%s",tr("Git index has unresolved conflicts"));
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("CyToolsCore / MCP")) mcpPanel_=true;
    if (ImGui::MenuItem(cyui::trLabel("Options").c_str())) optionsPanel_=true;
    if (ImGui::MenuItem(cyui::trLabel("Updates").c_str())) updatePanel_=true;
    ImGui::EndMainMenuBar();
}
void Reader::registerFilterOwner(cyui::TextFilter &filter) { filterOwners_[ImGui::GetCurrentWindow()->ID]=&filter; }
bool Reader::routeFilterFind() {
    for (auto *window=ImGui::GetCurrentContext()->NavWindow;window;window=window->ParentWindow) {
        if (pluginOutputChild_ && window->ChildId==pluginOutputChild_) return true; // native read-only editor owns Find
        const auto found=filterOwners_.find(window->ID);
        if (found!=filterOwners_.end()) {
            if (!ImGui::GetIO().WantTextInput) found->second->focus();
            return true;
        }
    }
    return false;
}
} // namespace cycode::reader
