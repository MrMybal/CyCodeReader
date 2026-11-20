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
    if (auto tab=active()) { if (tab->paged) throw std::runtime_error(tr("This command requires an ordinary text document."));sync(tab);params["id"]=tab->id;params["expectedRevision"]=tab->live->revision;auto result=core_.dispatch(method,params);if (method=="plugins.invoke") { pluginResult_=result.at("result").dump(2);pluginOutput_.setLanguage(cyui::CodeLanguage::json);configureCodeEditor(pluginOutput_,editorFont_);pluginOutput_.setText(pluginResult_);pluginOutput_.setReadOnly(true); }sync(tab); }
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
                    const Json input={{"text",std::string(source)}};
                    std::function<Json()> call;
                    { std::lock_guard lock(core_.mutex());call=core_.plugins().prepare(pluginId,command,input); }
                    // Core pins the DLL and serializes plugin execution without
                    // retaining the live-document/UI mutex during the callback.
                    return pluginSyntaxTokens(call());
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
    if(automation_)return;
    try {
        automationStopping_=false;
        const auto profile=options_.automationData.empty()?readerDataDirectory(options_.installation)/"automation/default":options_.automationData;
        Json settings=Json::object();
        const auto config=options_.automationConfig.empty()?profile/"settings.json":options_.automationConfig;
        if(std::filesystem::is_regular_file(config))settings=Json::parse(readTextFileBytes(config));
        sdkConfig_=config;sdkAccessProfile_=accessProfileName(AutomationOptions::fromJson(settings).accessProfile);
        sdkDescriptor_=options_.automationDescriptor.empty()?profile/"connection.json":options_.automationDescriptor;
#ifdef _WIN32
        cli_=options_.installation/"app"/"cycodereader_cli.exe";
#else
        cli_=options_.installation/"app"/"cycodereader_cli";
#endif
        automation_=std::make_shared<Automation>(profile,[this](const std::string &method,const Json &parameters,const cymcp::CallContext &context){
            auto request=std::make_shared<AutomationRequest>();request->method=method;request->parameters=parameters;request->context=context;
            auto result=request->reply.get_future();
            {std::lock_guard lock(automationMutex_);if(automationStopping_)throw cymcp::Error("Cancelled","Reader is shutting down");
             if(automationRequests_.size()>=128)throw cymcp::Error("Busy","Reader operation queue is full");automationRequests_.push_back(request);}
            return result.get();
        },true,AutomationOptions::fromJson(settings));
        automation_->start(sdkDescriptor_);assistant_=std::make_unique<Assistant>(*automation_,settings);
        sdkError_.clear();serviceLogs_.append(cyui::LogLevel::info,"CyMCPSDK native service ready","CyMCPSDK");
    } catch(const std::exception &e){sdkError_=cymcp::redact_error_message(e.what());automation_.reset();serviceLogs_.append(cyui::LogLevel::error,sdkError_,"CyMCPSDK");}
}
void Reader::extensionPanels() {
    if (pluginsPanel_) {
        const bool visible=ImGui::Begin((std::string(tr("Extensions"))+"###reader-plugins").c_str(),&pluginsPanel_);
        if (visible) {
            if (cyui::iconButton(cyui::icon::help,tr("Extension documentation"))) execute("help",{{"page","plugins"}});
            const auto state=core_.plugins().describe();
            cyui::textDim("%s",tr("Plugins load at startup. Restart Reader after changing manifests or libraries."));
            if (state.at("plugins").empty()) cyui::emptyState(tr("Extensions"),tr("Place plugin manifests and libraries in the Reader plugins folder."),cyui::icon::code);
            registerFilterOwner(pluginsFilter_);
            cyui::panelHeader("Extensions",nullptr,&pluginsFilter_);
            size_t matchingPlugins=0;
            for (const auto &plugin : state.at("plugins")) {
                const auto pluginId=plugin.at("id").get<std::string>(),name=plugin.at("name").get<std::string>();
                const auto context=name+" "+pluginId;
                auto &view=pluginOptions_[pluginId];view.synchronize(plugin);
                auto commandMatches=[&](const Json &command) { return command.value("kind","")!="syntax" && command.value("kind","")!="service" && pluginsFilter_.matches(command.at("title").get_ref<const std::string &>(),context+" "+command.at("id").get<std::string>()); };
                auto languageMatches=[&](const Json &language) {
                    auto keywords=context+" "+language.at("id").get<std::string>();
                    for (const auto &ext : language.at("extensions")) keywords+=" "+ext.get<std::string>();
                    return pluginsFilter_.matches(language.at("name").get_ref<const std::string &>(),keywords);
                };
                bool any=pluginsFilter_.matches(name,pluginId);
                for (const auto &command : plugin.at("commands")) any|=commandMatches(command);
                for (const auto &language : plugin.at("languages")) any|=languageMatches(language);
                for (const auto &property : view.properties) any|=cyui::propertyMatches(pluginsFilter_,property,context.c_str());
                if (any) ++matchingPlugins;
                ImGui::PushID(pluginId.c_str());
                if (cyui::beginFilteredSection(name.c_str(),pluginsFilter_,any)) {
                    cyui::textDim("%s | %s",pluginId.c_str(),plugin.at("version").get_ref<const std::string&>().c_str());
                    for (const auto &command : plugin.at("commands")) if (commandMatches(command)) {
                        ImGui::BeginDisabled(!active() || active()->editor.IsReadOnlyEnabled());
                        if (cyui::button(command.at("title").get_ref<const std::string&>().c_str())) execute("plugin",{{"plugin",pluginId},{"command",command.at("id")}});
                        ImGui::EndDisabled();
                    }
                    const auto edits=cyui::inspector("##plugin-options",view.state,view.properties,view.revision,
                        [&](auto id,const auto &value) { return view.validate(id,value); },{&pluginsFilter_,context.c_str()});
                    view.commit(core_.plugins(),pluginId,edits);
                    for (const auto &language : plugin.at("languages")) if (languageMatches(language)) cyui::textDim("%s",language.at("name").get_ref<const std::string&>().c_str());
                    cyui::endSection();
                } else cancelInspectorDrafts(view.state);
                ImGui::PopID();
            }
            if (!matchingPlugins && !state.at("plugins").empty()) cyui::textDim("%s",tr("No matching extensions"));
            for (const auto &issue : state.at("errors")) cyui::callout(cyui::Tone::warning,issue.dump().c_str());
            if (!pluginResult_.empty()) {
                ImGui::Separator();
                if (cyui::button(tr("Copy result"))) ImGui::SetClipboardText(pluginResult_.c_str());
                pluginOutputChild_=ImGui::GetID("##plugin-result");
                pluginOutput_.draw("##plugin-result",cyui::dp(0,180));
            }
        } else for (auto &[id,view] : pluginOptions_) cancelInspectorDrafts(view.state);
        ImGui::End();
    } else for (auto &[id,view] : pluginOptions_) cancelInspectorDrafts(view.state);
    if (mcpPanel_) {
        const bool visible=ImGui::Begin("CyMCPSDK / MCP",&mcpPanel_);
        if (visible && active() && active()->paged) cyui::callout(cyui::Tone::warning,tr("The SDK/MCP session currently exposes ordinary documents only."));
        if (visible) {
            if (cyui::iconButton(cyui::icon::help,tr("MCP documentation"))) execute("help",{{"page","mcp"}});
            const bool ready=bool(automation_) && !sdkDescriptor_.empty() && std::filesystem::is_regular_file(sdkDescriptor_);
            cyui::badge(tr(ready ? "SDK service ready" : "SDK service unavailable"),ready ? cyui::Tone::success : cyui::Tone::warning);

            if(ImGui::CollapsingHeader(tr("MCP connection"))){
            if (cyui::button(tr("Start SDK service"))) startSdk();
            if(ImGui::BeginCombo(tr("MCP access profile"),tr(sdkAccessProfile_=="free"?"Free":sdkAccessProfile_=="local_controlled"?"Local controlled":sdkAccessProfile_=="token_only"?"Token only":"Protected"))){
                for(const auto &item:std::vector<std::pair<const char*,const char*>>{{"free","Free"},{"local_controlled","Local controlled"},{"token_only","Token only"},{"protected","Protected"}}){
                    if(ImGui::Selectable(tr(item.second),sdkAccessProfile_==item.first))try{
                        std::optional<std::string> baseline;
                        if(std::filesystem::exists(sdkConfig_))baseline=readTextFileBytes(sdkConfig_);
                        auto settings=baseline?Json::parse(*baseline):Json::object();
                        settings.erase("access_profile");settings["accessProfile"]=item.first;
                        if(!std::filesystem::exists(sdkConfig_.parent_path()))cymcp::private_directory(sdkConfig_.parent_path());
                        writeTextFileAtomic(sdkConfig_,settings.dump(2)+"\n",baseline);sdkAccessProfile_=item.first;
                    }catch(const std::exception&e){sdkError_=cymcp::redact_error_message(e.what());}
                }ImGui::EndCombo();
            }
            cyui::textDim("%s",tr("MCP profile changes apply after restarting Reader."));
            if(ready){
                const auto state=automation_->status();
                cyui::textDim("%s: %s",tr("Active MCP profile"),state.at("accessProfile").get_ref<const std::string&>().c_str());
                if(cyui::button(tr("Copy MCP address"))){const auto connection=Json::parse(readTextFileBytes(sdkDescriptor_));ImGui::SetClipboardText(connection.at("mcpUrl").get_ref<const std::string&>().c_str());}
            }
            ImGui::BeginDisabled(!ready);
            const bool controlled=ready&&automation_->status().at("controlOperations").get<bool>();
            const bool tokenRequired=ready&&automation_->status().at("requireToken").get<bool>();
            for (bool edit : {false,true}) if ((!edit||(controlled&&tokenRequired))&&cyui::button(tr(!controlled ? "Copy MCP configuration" : edit ? "Copy editing MCP configuration" : "Copy read-only MCP configuration"))) {
                Json config={{"mcpServers",{{"CyCodeReader",{{"command",pathUtf8(cli_)},{"args",Json::array({"--connect",pathUtf8(sdkDescriptor_),"--access",edit ? "edit" : "read","mcp"})}}}}}};
                ImGui::SetClipboardText(config.dump(2).c_str());
            }ImGui::EndDisabled();
            cyui::textDim("%s",tr(controlled ? "The read identity cannot edit files or execute plugins. Saving remains explicit." : "All exposed MCP operations are available. Saving remains explicit."));
            }
            if(!sdkError_.empty())cyui::textDim("%s",sdkError_.c_str());

            if(ImGui::CollapsingHeader(tr("Service logs"))){
            if (cyui::button(tr("Copy logs"))) {
                std::vector<cyui::LogEntry> rows;serviceLogs_.snapshot(rows,0);std::string text;
                for (const auto &row : rows) text+=row.text+"\n";
                ImGui::SetClipboardText(text.c_str());
            }ImGui::SameLine();if (cyui::button(tr("Clear logs"))) serviceLogs_.clear();
            cyui::logView("##reader-service-log",serviceLogState_,serviceLogs_,cyui::dp(0,120));
            }
            if(assistant_)assistant_->draw();
        }ImGui::End();
    }
    if (optionsPanel_) {
        const bool visible=ImGui::Begin((std::string(tr("Options"))+"###reader-options").c_str(),&optionsPanel_);
        if (visible) {
            registerFilterOwner(optionsFilter_);
            cyui::panelHeader("Options",nullptr,&optionsFilter_);
            const Json current={{"language",locale_},{"theme",theme_},{"density",density_},{"scale",textScale_}};
            if (optionsModel_!=current) { optionsModel_=current;++optionsRevision_; }
            std::vector<cyui::Property> properties;
            auto choice=[&](const char *key,const char *label,std::string value,std::vector<std::string> choices) {
                cyui::Property property;property.id=uiId(std::string("options/")+key);property.label=label;
                property.value=std::move(value);property.choices=std::move(choices);properties.push_back(std::move(property));
            };
            choice("language","Language",locale_,{"en","fr"});
            choice("theme","Theme",theme_,{"graphite","mid-grey","light-grey","amber-night","violet","navy"});
            choice("density","Density",density_,{"compact","comfortable"});
            cyui::Property scale;scale.id=uiId("options/scale");scale.label="Text size";scale.value=double(textScale_);
            scale.bounded=true;scale.minimum=.8;scale.maximum=2.;properties.push_back(scale);
            for (const auto &proposal : cyui::inspector("##reader-options",optionsState_,properties,optionsRevision_,{},{&optionsFilter_,"Options"})) {
                if (!proposal.edit.committed()) continue;
                if (proposal.id==uiId("options/language")) locale_=std::get<std::string>(proposal.edit.value);
                else if (proposal.id==uiId("options/theme")) theme_=std::get<std::string>(proposal.edit.value);
                else if (proposal.id==uiId("options/density")) density_=std::get<std::string>(proposal.edit.value);
                else if (proposal.id==uiId("options/scale")) textScale_=float(std::clamp(std::get<double>(proposal.edit.value),.8,2.));
                applyAppearance();
            }
            std::vector<std::filesystem::path> licenses;std::error_code ec;
            for (const auto &file : std::filesystem::directory_iterator(options_.installation/"licenses",ec)) {
                if (file.path().extension()==".txt") licenses.push_back(file.path());
            }
            for (const auto *name : {"LICENSE", "COPYRIGHT"}) {
                const auto file=options_.installation/name;
                if (std::filesystem::is_regular_file(file,ec)) licenses.push_back(file);
            }
            bool matchingLicense=optionsFilter_.matches("CyCodeReader GPL-3.0-only Copyright", "Licenses "+cyui::tr("Licenses"));
            for (const auto &file : licenses) matchingLicense|=optionsFilter_.matches(pathUtf8(file.filename()),"Licenses "+cyui::tr("Licenses"));
            if (cyui::beginFilteredSection("Licenses",optionsFilter_,matchingLicense,false)) {
                ImGui::TextWrapped("%s",cyui::tr("CyCodeReader: GPL-3.0-only. Copyright (c) 2026 Cyberalien. This program comes without any warranty. See LICENSE and COPYRIGHT.").c_str());
                for (const auto &file : licenses) if (optionsFilter_.matches(pathUtf8(file.filename()),"Licenses "+cyui::tr("Licenses"))) {
                    if (ImGui::TreeNode(pathUtf8(file.filename()).c_str())) { const auto text=readTextFileBytes(file);ImGui::TextUnformatted(text.c_str());ImGui::TreePop(); }
                }
                cyui::endSection();
            }
        } else cancelInspectorDrafts(optionsState_);
        ImGui::End();
    } else cancelInspectorDrafts(optionsState_);
}
}
