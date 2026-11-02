#include "Reader.h"
#include "PluginLanguage.h"
#include "cycode/EditorTheme.h"
#include "cycode/Paths.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace cycode::reader {
void Reader::nativeAction(const std::string &method,Json params) {
    if (auto tab=active()) { sync(tab);params["id"]=tab->id;params["expectedRevision"]=tab->live->revision;auto result=core_.dispatch(method,params);if (method=="plugins.invoke") { pluginResult_=result.at("result").dump(2);pluginOutput_.setLanguage(cyui::CodeLanguage::json);configureCodeEditor(pluginOutput_);pluginOutput_.setText(pluginResult_);pluginOutput_.setReadOnly(true); }sync(tab); }
}
void Reader::preparePluginLanguages() {
    const auto state=core_.plugins().describe();
    for (const auto &issue : state.at("errors")) serviceLogs_.append(cyui::LogLevel::warning,issue.dump(),"Plugins");
    for (const auto &plugin : state.at("plugins")) for (const auto &definition : plugin.at("languages")) {
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
        try {
            const auto pluginId=plugin.at("id").get<std::string>();
            const auto key=pluginId+"/"+definition.at("id").get<std::string>();
            auto shared=sharedPluginLanguage(key,definition);
            if (definition.contains("providerCommand")) {
                const auto command=definition.at("providerCommand").get<std::string>();
                shared.highlight=[this,pluginId,command](std::string_view source) {
                    std::lock_guard lock(core_.mutex());
                    return pluginSyntaxTokens(core_.plugins().invoke(pluginId,command,{{"text",std::string(source)}}));
                };
            }
            pluginSyntax_[key]=cyui::registerCodeLanguage(shared);
        } catch (const std::exception &e) { serviceLogs_.append(cyui::LogLevel::error,e.what(),"Plugin syntax"); }
#else
        auto language=makePluginLanguage(definition);
        const auto key=plugin.at("id").get<std::string>()+"/"+definition.at("id").get<std::string>();
        for (const auto &ext : definition.at("extensions")) pluginExtensions_[ext.get<std::string>()]=key;
        pluginLanguages_.emplace(key,std::move(language));
#endif
    }
}
void Reader::startSdk() {
    if (sdk_.valid() && sdk_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
    if (sdk_.valid()) sdk_.get();sdkStop_=false;sdkError_.clear();
    serviceLogs_.append(cyui::LogLevel::info,"Starting CyToolsCore SDK service","CyToolsCore");
    try {
        if (!bridge_) {
#ifdef _WIN32
            const auto pid=GetCurrentProcessId();
#else
            const auto pid=getpid();
#endif
            const auto directory=options_.installation/"data/bridge"/std::to_string(pid);
            bridge_=std::make_unique<Bridge>(core_,directory/"native.json");sdkDescriptor_=directory/"core.json";
        }
        python_.clear();
        if (const auto value=std::getenv("CYCODE_READER_PYTHON")) python_=utf8Path(value);
        else for (auto path=options_.installation;!path.empty();path=path.parent_path()) {
#ifdef _WIN32
            const auto candidate=path/"runtime/python/Scripts/python.exe";
#else
            const auto candidate=path/"runtime/python/bin/python";
#endif
            if (std::filesystem::is_regular_file(candidate)) { python_=candidate;break; }if (path==path.parent_path()) break;
        }
        if (python_.empty() || !std::filesystem::is_regular_file(python_)) throw std::runtime_error(tr("Install the Reader SDK runtime with scripts/setup_sdk.py."));
        const auto script=options_.installation/"app/tool.py";
        if (!std::filesystem::is_regular_file(script)) throw std::runtime_error("Reader SDK app/tool.py is missing");
        std::vector<std::string> argv={pathUtf8(python_),pathUtf8(script),"--desktop-service","--native-descriptor",pathUtf8(bridge_->descriptor()),"--service-descriptor",pathUtf8(sdkDescriptor_),"--data-dir",pathUtf8(sdkDescriptor_.parent_path()/"jobs")};
        sdk_=std::async(std::launch::async,[this,argv] { return runProcess(argv,options_.installation,std::chrono::hours(24),256*1024,&sdkStop_); });
    } catch (const std::exception &e) { sdkError_=e.what();serviceLogs_.append(cyui::LogLevel::error,sdkError_,"CyToolsCore"); }
}
void Reader::extensionPanels() {
    if (searchPanel_) {
        const bool visible=ImGui::Begin((std::string(tr("Search open documents"))+"###reader-search").c_str(),&searchPanel_);
        if (visible) {
            ImGui::InputText(tr("Search text"),&searchQuery_);
            ImGui::BeginDisabled(searchQuery_.empty());
            if (cyui::primaryButton(tr("Find all"))) {
                searchMatches_=core_.dispatch("search.openDocuments",{{"query",searchQuery_}});
                searchRows_.clear();searchNavigation_.clear();
                for (const auto &match : searchMatches_.value("matches",Json::array())) {
                    const auto tab=find(match.at("id"));if (!tab) continue;
                    const auto id=uiId("search/"+std::to_string(tab->id)+"/"+match.at("revision").dump()+"/"+match.at("offset").dump());
                    const auto source=tab->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->document.path());
                    searchRows_.push_back({id,cyui::LogLevel::info,source,searchQuery_,match.at("line"),match.at("column")});
                    searchNavigation_.emplace(id,match);
                }
            }ImGui::EndDisabled();
            if (searchMatches_.value("truncated",false)) cyui::callout(cyui::Tone::warning,tr("Search results limited to 2000 matches."));
            const auto request=cyui::diagnosticList("##reader-search-results",searchRows_,{0,0});
            if (searchNavigation_.contains(request)) {
                const auto &match=searchNavigation_.at(request);const auto tab=find(match.at("id"));
                if (tab && match.at("revision")==tab->live->revision) {
                    const auto &text=tab->document.text();const auto end=match.at("offset").get<size_t>();
                    const auto line=text.rfind('\n',end);size_t glyphs=0;
                    for (size_t byte=line==std::string::npos ? 0 : line+1;byte<end;++byte)
                        if ((static_cast<unsigned char>(text[byte])&0xc0)!=0x80) ++glyphs;
                    active_=tab->id;tab->focus=true;tab->editor.SetCursor({match.at("line").get<size_t>()-1,glyphs});
                } else { searchMatches_=Json::object();searchRows_.clear();searchNavigation_.clear(); }
            }
        }ImGui::End();
    }
    if (sdk_.valid() && sdk_.wait_for(std::chrono::seconds(0))==std::future_status::ready) { const auto result=sdk_.get();sdkError_=result.output.empty() ? tr("The Reader SDK service stopped.") : result.output;serviceLogs_.append(cyui::LogLevel::error,sdkError_,"CyToolsCore"); }
    if (pluginsPanel_) {
        const bool visible=ImGui::Begin((std::string(tr("Extensions"))+"###reader-plugins").c_str(),&pluginsPanel_);
        if (visible) {
            const auto state=core_.plugins().describe();
            cyui::textDim("%s",tr("Plugins load at startup. Restart Reader after changing manifests or libraries."));
            if (state.at("plugins").empty()) cyui::emptyState(tr("Extensions"),tr("Place plugin manifests and libraries in the Reader plugins folder."),cyui::icon::code);
            for (const auto &plugin : state.at("plugins")) {
                ImGui::PushID(plugin.at("id").get_ref<const std::string&>().c_str());
                if (ImGui::CollapsingHeader(plugin.at("name").get_ref<const std::string&>().c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
                    cyui::textDim("%s | %s",plugin.at("id").get_ref<const std::string&>().c_str(),plugin.at("version").get_ref<const std::string&>().c_str());
                    for (const auto &command : plugin.at("commands")) {
                        if (command.value("kind","")=="syntax") continue;
                        ImGui::BeginDisabled(!active() || active()->editor.IsReadOnlyEnabled());
                        if (cyui::button(command.at("title").get_ref<const std::string&>().c_str())) execute("plugin",{{"plugin",plugin.at("id")},{"command",command.at("id")}});
                        ImGui::EndDisabled();
                    }
                    const auto pluginId=plugin.at("id").get<std::string>();
                    auto &view=pluginOptions_[pluginId];view.synchronize(plugin);
                    const auto edits=cyui::inspector("##plugin-options",view.state,view.properties,view.revision,
                        [&](auto id,const auto &value) { return view.validate(id,value); });
                    view.commit(core_.plugins(),pluginId,edits);
                    for (const auto &language : plugin.at("languages")) cyui::textDim("%s",language.at("name").get_ref<const std::string&>().c_str());
                }ImGui::PopID();
            }
            for (const auto &issue : state.at("errors")) cyui::callout(cyui::Tone::warning,issue.dump().c_str());
            if (!pluginResult_.empty()) {
                ImGui::Separator();
                if (cyui::button(tr("Copy result"))) ImGui::SetClipboardText(pluginResult_.c_str());
                pluginOutput_.draw("##plugin-result",cyui::dp(0,180));
            }
        }ImGui::End();
    }
    if (mcpPanel_) {
        const bool visible=ImGui::Begin("CyToolsCore / MCP",&mcpPanel_);
        if (visible) {
            const bool ready=sdk_.valid() && !sdkDescriptor_.empty() && std::filesystem::is_regular_file(sdkDescriptor_);
            cyui::badge(tr(ready ? "SDK service ready" : "SDK service unavailable"),ready ? cyui::Tone::success : cyui::Tone::warning);

            if (cyui::button(tr("Start SDK service"))) startSdk();
            ImGui::BeginDisabled(!ready);
            for (bool edit : {false,true}) if (cyui::button(tr(edit ? "Copy editing MCP configuration" : "Copy read-only MCP configuration"))) {
                Json config={{"mcpServers",{{"CyCodeReader",{{"command",pathUtf8(python_)},{"args",Json::array({pathUtf8(options_.installation/"app/tool.py"),"--bridge-config",pathUtf8(sdkDescriptor_),"--access",edit ? "edit" : "read","mcp"})}}}}}};
                ImGui::SetClipboardText(config.dump(2).c_str());
            }ImGui::EndDisabled();
            cyui::textDim("%s",tr("The read identity cannot edit files or execute plugins. Saving remains explicit."));
            if (cyui::button(tr("Copy logs"))) {
                std::vector<cyui::LogEntry> rows;serviceLogs_.snapshot(rows,0);std::string text;
                for (const auto &row : rows) text+=row.text+"\n";
                ImGui::SetClipboardText(text.c_str());
            }ImGui::SameLine();if (cyui::button(tr("Clear logs"))) serviceLogs_.clear();
            cyui::logView("##reader-service-log",serviceLogState_,serviceLogs_,{0,0});
        }ImGui::End();
    }
    if (optionsPanel_) {
        const bool visible=ImGui::Begin((std::string(tr("Options"))+"###reader-options").c_str(),&optionsPanel_);
        if (visible) {
            const Json current={{"language",locale_},{"theme",theme_},{"density",density_},{"scale",textScale_}};
            if (optionsModel_!=current) { optionsModel_=current;++optionsRevision_; }
            std::vector<cyui::Property> properties;
            auto choice=[&](const char *key,const char *label,std::string value,std::vector<std::string> choices) {
                cyui::Property property;property.id=uiId(std::string("options/")+key);property.label=tr(label);
                property.value=std::move(value);property.choices=std::move(choices);properties.push_back(std::move(property));
            };
            choice("language","Language",locale_,{"en","fr"});
            choice("theme","Theme",theme_,{"graphite","mid-grey","light-grey","amber-night","violet","navy"});
            choice("density","Density",density_,{"compact","comfortable"});
            cyui::Property scale;scale.id=uiId("options/scale");scale.label=tr("Text size");scale.value=double(textScale_);
            scale.bounded=true;scale.minimum=.8;scale.maximum=2.;properties.push_back(scale);
            for (const auto &proposal : cyui::inspector("##reader-options",optionsState_,properties,optionsRevision_)) {
                if (!proposal.edit.committed()) continue;
                if (proposal.id==uiId("options/language")) locale_=std::get<std::string>(proposal.edit.value);
                else if (proposal.id==uiId("options/theme")) theme_=std::get<std::string>(proposal.edit.value);
                else if (proposal.id==uiId("options/density")) density_=std::get<std::string>(proposal.edit.value);
                else if (proposal.id==uiId("options/scale")) textScale_=float(std::clamp(std::get<double>(proposal.edit.value),.8,2.));
                applyAppearance();
            }
            if (ImGui::CollapsingHeader(tr("Licenses"))) {
                ImGui::TextWrapped("%s",tr("CyCodeReader: GPL-3.0-only. Copyright (c) 2026 Cyberalien. This program comes without any warranty. See LICENSE and COPYRIGHT."));
                std::error_code projectLicenseError;
                for (const auto *name : {"LICENSE", "COPYRIGHT"}) {
                    const auto file=options_.installation/name;
                    if (std::filesystem::is_regular_file(file,projectLicenseError) && ImGui::TreeNode(name)) {
                        const auto text=readTextFileBytes(file);ImGui::TextWrapped("%s",text.c_str());ImGui::TreePop();
                    }
                }
                std::error_code ec;for (const auto &file : std::filesystem::directory_iterator(options_.installation/"licenses",ec)) {
                    if (file.path().extension()!=".txt") continue;
                    if (ImGui::TreeNode(file.path().filename().string().c_str())) { const auto text=readTextFileBytes(file.path());ImGui::TextUnformatted(text.c_str());ImGui::TreePop(); }
                }
            }
        }ImGui::End();
    }
}
}
