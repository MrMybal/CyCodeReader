#include "Assistant.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <SDL3/SDL.h>
#include <cstdlib>
#include <algorithm>
namespace cycode::reader {
Assistant::Assistant(Automation &automation,Json settings)
    :automation_(automation),settings_(std::move(settings)),providers_(std::make_shared<cymcp::ProviderRegistry>()),
     controller_(providers_),panel_(controller_,[&]{cymcp::ChatPanelOptions p;
        p.open_link=[](const auto &url){SDL_OpenURL(url.c_str());};p.open_settings=[this]{settingsOpen_=true;};return p;}()){
    const auto chat=settings_.value("chat",Json::object());provider_=chat.value("provider",provider_);model_=chat.value("model","");
    endpoint_=chat.value("baseUrl",endpoint_);keyEnv_=chat.value("apiKeyEnv","");compact_=chat.value("compact",true);
    contextKiB_=chat.value("contextKiB",contextKiB_);recentTurns_=chat.value("recentTurns",recentTurns_);contextEnabled_=chat.value("contextEnabled",false);
}
Assistant::~Assistant(){cancel();remoteCancel_.cancel();if(remoteConnection_.valid())remoteConnection_.wait();discoveryCancel_.cancel();if(discovery_.valid())discovery_.wait();controller_.disconnect();}
void Assistant::cancel(){controller_.cancel();}
void Assistant::poll(){
    controller_.poll();
    if(remoteConnection_.valid()&&remoteConnection_.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{remoteConnection_.get();remotesConnected_=true;}catch(const std::exception&e){error_=cymcp::redact_error_message(e.what());}}
    if(discovery_.valid()&&discovery_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{detected_=discovery_.get();}catch(const std::exception&e){error_=cymcp::redact_error_message(e.what());}
    }
}
void Assistant::openConfigured(bool restore){
    cymcp::ChatConnection connection;connection.provider_id=provider_;connection.model=model_;
    connection.system_prompt="You assist the user inside CyCodeReader. Use Reader tools for its live documents and view state. Read current revisions before edits. Saving and destructive discards must be explicitly requested. Never replay an action after cancellation or an unknown outcome; inspect durable job receipts.";
    if(selected_<detected_.size()&&detected_[selected_].provider_id==provider_)connection.provider=detected_[selected_].config;
    const auto chat=settings_.value("chat",Json::object());
    if(chat.contains("command"))connection.provider.process.executable=chat.at("command").get<std::string>();
    connection.provider.process.cwd=automation_.dataDirectory();
    connection.provider.settings["base_url"]=endpoint_;connection.provider.settings["sandbox"]="read-only";
    connection.provider.settings["tool_discovery_threshold"]=compact_?24:65536;
    if(!keyEnv_.empty()){const auto value=std::getenv(keyEnv_.c_str());connection.provider.api_key=value?value:"";}
    connection.provider.tools=automation_.tools(allowEdits_?"chat-edit":"chat-read",compact_);
    connection.provider.approvals=cymcp::Policy::deny_all();
    if(remember_){connection.history_directory=automation_.dataDirectory()/"conversations";connection.history_id=chat.value("historyId","reader-chat");}
    connection.restore=restore;connection.context.max_bytes=contextEnabled_?size_t(std::clamp(contextKiB_,16,8192))*1024:0;
    connection.context.recent_turns=size_t(std::clamp(recentTurns_,1,100));controller_.open(std::move(connection));settingsOpen_=false;
}
void Assistant::connectRemotes(){
    if(remoteConnection_.valid()||remotesConnected_)throw cymcp::Error("Busy","Remote discovery is already active or complete");
    const auto servers=settings_.value("remoteServers",Json::array());
    remoteConnection_=std::async(std::launch::async,[this,servers]{automation_.connectRemotes(servers,remoteCancel_);});
}
void Assistant::send(std::string text,std::vector<std::filesystem::path> attachments){controller_.send(std::move(text),std::move(attachments));}
Json Assistant::state() const{
    const auto&s=controller_.state();Json messages=Json::array();for(const auto&m:s.messages)messages.push_back({{"role",m.role},{"text",m.markdown}});
    return {{"phase",s.phase},{"busy",s.busy},{"connected",s.connected},{"messages",messages},{"error",s.error_code},{"provider",s.provider_id},{"models",s.models}};
}
void Assistant::draw(){
    poll();
    if(!settingsOpen_&&cyui::button(cyui::trLabel("Assistant connection").c_str()))settingsOpen_=true;
    if(settingsOpen_){
        if(ImGui::CollapsingHeader(cyui::trLabel("Assistant connection").c_str(),ImGuiTreeNodeFlags_DefaultOpen)){
            if(cyui::button(cyui::trLabel("Detect providers").c_str())&&!discovery_.valid()){
                auto providers=providers_;auto cancellation=discoveryCancel_;
                discovery_=std::async(std::launch::async,[providers,cancellation]{cymcp::CallContext c;c.cancellation=cancellation;return providers->discover({},c);});
            }
            if(!detected_.empty()&&ImGui::BeginCombo(cyui::trLabel("Detected provider").c_str(),provider_.c_str())){
                for(size_t i=0;i<detected_.size();++i){const auto &item=detected_[i];
                    if(ImGui::Selectable((item.name+" ("+item.state+")##"+std::to_string(i)).c_str(),i==selected_)){
                        selected_=i;provider_=item.provider_id;endpoint_=item.config.settings.value("base_url",endpoint_);
                    }
                }ImGui::EndCombo();
            }
            if(settings_.contains("remoteServers")){
                ImGui::BeginDisabled(remoteConnection_.valid()||remotesConnected_);
                if(cyui::button(cyui::trLabel("Connect configured MCP servers").c_str()))try{connectRemotes();}catch(const std::exception&e){error_=cymcp::redact_error_message(e.what());}
                ImGui::EndDisabled();cyui::textDim("%s",cyui::tr("After remote discovery, open a new chat connection to refresh its tools.").c_str());
            }
            ImGui::InputText(cyui::trLabel("Provider ID").c_str(),&provider_);ImGui::InputText(cyui::trLabel("Model").c_str(),&model_);
            ImGui::InputText(cyui::trLabel("Endpoint").c_str(),&endpoint_);ImGui::InputText(cyui::trLabel("API key environment variable").c_str(),&keyEnv_);
            const auto &current=controller_.state();
            ImGui::BeginDisabled(!current.connected||current.busy);
            if(cyui::button(cyui::trLabel("Refresh models").c_str()))try{refreshModels();}catch(const std::exception&e){error_=cymcp::redact_error_message(e.what());}
            ImGui::EndDisabled();
            if(current.models.is_array()&&!current.models.empty()&&ImGui::BeginCombo(cyui::trLabel("Available models").c_str(),model_.c_str())){
                for(const auto &entry:current.models){const auto id=entry.is_string()?entry.get<std::string>():entry.value("id",std::string{});
                    if(!id.empty()&&ImGui::Selectable(id.c_str(),id==model_)){model_=id;if(current.connected&&current.turn_model_selection)controller_.select_model(id);}
                }ImGui::EndCombo();
            }
            ImGui::Checkbox(cyui::trLabel("Allow document edits").c_str(),&allowEdits_);
            ImGui::Checkbox(cyui::trLabel("Keep private chat history").c_str(),&remember_);ImGui::SameLine();ImGui::Checkbox(cyui::trLabel("Compact tool discovery").c_str(),&compact_);
            ImGui::Checkbox(cyui::trLabel("Limit API context window").c_str(),&contextEnabled_);
            if(contextEnabled_){ImGui::InputInt(cyui::trLabel("Context size (KiB)").c_str(),&contextKiB_);ImGui::InputInt(cyui::trLabel("Recent turns").c_str(),&recentTurns_);}
            ImGui::BeginDisabled(current.busy);
            if(cyui::button(cyui::trLabel("Connect / new chat").c_str()))try{openConfigured();error_.clear();}catch(const std::exception&e){error_=cymcp::redact_error_message(e.what());}
            ImGui::SameLine();if(cyui::button(cyui::trLabel("Restore chat").c_str()))try{openConfigured(true);error_.clear();}catch(const std::exception&e){error_=cymcp::redact_error_message(e.what());}
            ImGui::SameLine();if(cyui::button(cyui::trLabel("Reconnect").c_str()))try{controller_.reconnect();}catch(const std::exception&e){error_=cymcp::redact_error_message(e.what());}
            ImGui::EndDisabled();
            ImGui::InputText(cyui::trLabel("Attachment path").c_str(),&attachment_);ImGui::SameLine();
            if(cyui::button(cyui::trLabel("Attach file").c_str())&&!attachment_.empty()){panel_.add_attachments({std::filesystem::u8path(attachment_)});attachment_.clear();}
            if(!error_.empty())cyui::textDim("%s",error_.c_str());
            cyui::textDim("%s",cyui::tr("Changing permissions applies when opening a new connection.").c_str());
        }
    }
    panel_.draw("##reader-chat");
}
}
