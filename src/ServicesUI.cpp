#include "Reader.h"
#include "PluginLanguage.h"
#include "cycode/EditorTheme.h"
#include "cycode/Paths.h"
#include <cyui/CyUI.h>
#include <imgui_stdlib.h>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace cycode::reader {
void Reader::nativeAction(const std::string &method,Json params) {
    if (auto tab=active()) { sync(tab);params["id"]=tab->id;params["expectedRevision"]=tab->live->revision;auto result=core_.dispatch(method,params);if (method=="plugins.invoke") pluginResult_=result.at("result").dump(2);sync(tab); }
}
void Reader::preparePluginLanguages() {
    const auto state=core_.plugins().describe();
    for (const auto &plugin : state.at("plugins")) for (const auto &definition : plugin.at("languages")) {
        auto language=makePluginLanguage(definition);
        const auto key=plugin.at("id").get<std::string>()+"/"+definition.at("id").get<std::string>();
        for (const auto &ext : definition.at("extensions")) pluginExtensions_[ext.get<std::string>()]=key;
        pluginLanguages_.emplace(key,std::move(language));
    }
}
void Reader::startSdk() {
    if (sdk_.valid() && sdk_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
    if (sdk_.valid()) sdk_.get();sdkStop_=false;sdkError_.clear();
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
    } catch (const std::exception &e) { sdkError_=e.what(); }
}
void Reader::extensionPanels() {
    if (searchPanel_) {
        const bool visible=ImGui::Begin(tr("Search open documents"),&searchPanel_);
        if (visible) {
            ImGui::InputText(tr("Search text"),&searchQuery_);
            ImGui::BeginDisabled(searchQuery_.empty());if (cyui::primaryButton(tr("Find all"))) searchMatches_=core_.dispatch("search.openDocuments",{{"query",searchQuery_}});ImGui::EndDisabled();
            for (const auto &match : searchMatches_.value("matches",Json::array())) {
                const auto tab=find(match.at("id"));if (!tab) continue;
                const auto label=pathUtf8(tab->document.path().filename())+":"+std::to_string(match.at("line").get<int>())+":"+std::to_string(match.at("column").get<int>());
                ImGui::PushID(static_cast<int>(tab->id));ImGui::PushID(static_cast<int>(match.at("offset").get<size_t>()));
                if (ImGui::Selectable(label.c_str())) {
                    if (match.at("revision")!=tab->live->revision) { searchMatches_=Json::object();ImGui::PopID();ImGui::PopID();break; }
                    active_=tab->id;tab->focus=true;tab->editor.SetCursor({match.at("line").get<size_t>()-1,0});
                }ImGui::PopID();ImGui::PopID();
            }
        }ImGui::End();
    }
    if (sdk_.valid() && sdk_.wait_for(std::chrono::seconds(0))==std::future_status::ready) { const auto result=sdk_.get();sdkError_=result.output.empty() ? tr("The Reader SDK service stopped.") : result.output; }
    if (pluginsPanel_) {
        const bool visible=ImGui::Begin(tr("Extensions"),&pluginsPanel_);
        if (visible) {
            const auto state=core_.plugins().describe();
            cyui::textDim("%s",tr("Plugins load at startup. Restart Reader after changing manifests or libraries."));
            if (state.at("plugins").empty()) cyui::emptyState(tr("Extensions"),tr("Place plugin manifests and libraries in the Reader plugins folder."),cyui::icon::code);
            for (const auto &plugin : state.at("plugins")) {
                ImGui::PushID(plugin.at("id").get_ref<const std::string&>().c_str());
                if (ImGui::CollapsingHeader(plugin.at("name").get_ref<const std::string&>().c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
                    cyui::textDim("%s | %s",plugin.at("id").get_ref<const std::string&>().c_str(),plugin.at("version").get_ref<const std::string&>().c_str());
                    for (const auto &command : plugin.at("commands")) {
                        ImGui::BeginDisabled(!active() || active()->editor.IsReadOnlyEnabled());
                        if (cyui::button(command.at("title").get_ref<const std::string&>().c_str())) execute("plugin",{{"plugin",plugin.at("id")},{"command",command.at("id")}});
                        ImGui::EndDisabled();
                    }
                    if (cyui::beginProperties("##plugin-options",cyui::dp(160))) {
                        for (const auto &definition : plugin.at("optionDefinitions")) {
                            const auto key=definition.at("id").get<std::string>();auto value=plugin.at("options").at(key);bool changed=false;
                            cyui::property(key.c_str());ImGui::PushID(key.c_str());
                            if (value.is_boolean()) { auto typed=value.get<bool>();changed=ImGui::Checkbox("##value",&typed);value=typed; }
                            else if (value.is_number_integer()) { auto typed=value.get<int>();changed=ImGui::InputInt("##value",&typed);value=typed; }
                            else if (value.is_string()) { auto typed=value.get<std::string>();changed=ImGui::InputText("##value",&typed);value=typed; }
                            if (changed && (!value.is_string() || value.get_ref<const std::string&>().size()<=4096)) core_.plugins().configure(plugin.at("id"),{{key,value}});ImGui::PopID();
                        }cyui::endProperties();
                    }
                    for (const auto &language : plugin.at("languages")) cyui::textDim("%s",language.at("name").get_ref<const std::string&>().c_str());
                }ImGui::PopID();
            }
            for (const auto &issue : state.at("errors")) cyui::callout(cyui::Tone::warning,issue.dump().c_str());
            if (!pluginResult_.empty()) { ImGui::Separator();ImGui::InputTextMultiline("##plugin-result",&pluginResult_,ImVec2(-1,cyui::dp(120)),ImGuiInputTextFlags_ReadOnly); }
        }ImGui::End();
    }
    if (mcpPanel_) {
        const bool visible=ImGui::Begin("CyToolsCore / MCP",&mcpPanel_);
        if (visible) {
            const bool ready=sdk_.valid() && !sdkDescriptor_.empty() && std::filesystem::is_regular_file(sdkDescriptor_);
            cyui::badge(tr(ready ? "SDK service ready" : "SDK service unavailable"),ready ? cyui::Tone::success : cyui::Tone::warning);
            if (!sdkError_.empty()) { auto message=sdkError_;ImGui::InputTextMultiline("##sdk-error",&message,ImVec2(-1,cyui::dp(100)),ImGuiInputTextFlags_ReadOnly); }
            if (cyui::button(tr("Start SDK service"))) startSdk();
            ImGui::BeginDisabled(!ready);
            for (bool edit : {false,true}) if (cyui::button(tr(edit ? "Copy editing MCP configuration" : "Copy read-only MCP configuration"))) {
                Json config={{"mcpServers",{{"CyCodeReader",{{"command",pathUtf8(python_)},{"args",Json::array({pathUtf8(options_.installation/"app/tool.py"),"--bridge-config",pathUtf8(sdkDescriptor_),"--access",edit ? "edit" : "read","mcp"})}}}}}};
                ImGui::SetClipboardText(config.dump(2).c_str());
            }ImGui::EndDisabled();
            cyui::textDim("%s",tr("The read identity cannot edit files or execute plugins. Saving remains explicit."));
        }ImGui::End();
    }
    if (optionsPanel_) {
        const bool visible=ImGui::Begin(tr("Options"),&optionsPanel_);
        if (visible && cyui::beginProperties("##reader-options",cyui::dp(140))) {
            cyui::property(tr("Language"));if (ImGui::Button(locale_=="en" ? "English" : "Français")) locale_=locale_=="en" ? "fr" : "en";
            cyui::property(tr("Theme"));if (ImGui::BeginCombo("##theme",theme_.c_str())) { for (const auto *key : {"graphite","mid-grey","light-grey","amber-night","violet","navy"}) if (ImGui::Selectable(key,theme_==key)) { theme_=key;applyEditorTheme(theme_); }ImGui::EndCombo(); }
            cyui::property(tr("Text size"));if (ImGui::SliderFloat("##scale",&textScale_,.8f,2.f,"%.2f")) ImGui::GetStyle().FontScaleMain=textScale_;
            cyui::endProperties();
            if (ImGui::CollapsingHeader(tr("Licenses"))) {
                std::error_code ec;for (const auto &file : std::filesystem::directory_iterator(options_.installation/"licenses",ec)) {
                    if (file.path().extension()!=".txt") continue;
                    if (ImGui::TreeNode(file.path().filename().string().c_str())) { const auto text=readTextFileBytes(file.path());ImGui::TextUnformatted(text.c_str());ImGui::TreePop(); }
                }
            }
        }ImGui::End();
    }
}
}
