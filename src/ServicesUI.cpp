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
    if (auto tab=active()) { sync(tab);params["id"]=tab->id;params["expectedRevision"]=tab->live->revision;auto result=core_.dispatch(method,params);if (method=="plugins.invoke") { pluginResult_=result.at("result").dump(2);pluginOutput_.setLanguage(cyui::CodeLanguage::json);configureCodeEditor(pluginOutput_,editorFont_);pluginOutput_.setText(pluginResult_);pluginOutput_.setReadOnly(true); }sync(tab); }
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
            const auto directory=readerDataDirectory(options_.installation)/"bridge"/std::to_string(pid);
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
                auto commandMatches=[&](const Json &command) { return command.value("kind","")!="syntax" && command.value("kind","")!="service" && pluginsFilter_.matches(command.at("title"),context+" "+command.at("id").get<std::string>()); };
                auto languageMatches=[&](const Json &language) {
                    auto keywords=context+" "+language.at("id").get<std::string>();
                    for (const auto &ext : language.at("extensions")) keywords+=" "+ext.get<std::string>();
                    return pluginsFilter_.matches(language.at("name"),keywords);
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
        const bool visible=ImGui::Begin("CyToolsCore / MCP",&mcpPanel_);
        if (visible) {
            if (cyui::iconButton(cyui::icon::help,tr("MCP documentation"))) execute("help",{{"page","mcp"}});
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
                ImGui::TextWrapped("%s",cyui::tr("CyCodeReader: GPL-3.0-only. Copyright (c) 2026 Cyberalien. This program comes without any warranty. See LICENSE and COPYRIGHT."));
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
