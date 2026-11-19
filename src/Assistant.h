#pragma once
#include "Automation.h"
#include <cymcp/chat_ui.hpp>
namespace cycode::reader {
class Assistant {
public:
    Assistant(Automation&,Json settings);
    ~Assistant();
    void draw();
    void poll();
    void cancel();
    void openConfigured(bool restore=false);
    void send(std::string,std::vector<std::filesystem::path> = {});
    void connectRemotes();
    void refreshModels(){controller_.refresh_models();}
    void setAllowEdits(bool value){allowEdits_=value;}
    Json state() const;
private:
    Automation &automation_;
    Json settings_;
    std::shared_ptr<cymcp::ProviderRegistry> providers_;
    cymcp::ChatController controller_;
    cymcp::ChatPanel panel_;
    cymcp::Cancellation discoveryCancel_,remoteCancel_;
    std::future<void> remoteConnection_;
    bool remotesConnected_=false;
    std::future<std::vector<cymcp::DiscoveredConnection>> discovery_;
    std::vector<cymcp::DiscoveredConnection> detected_;
    size_t selected_=0;
    bool settingsOpen_=true,allowEdits_=false,remember_=true,compact_=true,contextEnabled_=false;
    int contextKiB_=256,recentTurns_=8;
    std::string provider_="openai-compatible",model_,endpoint_="http://127.0.0.1:11434/v1",keyEnv_,attachment_,error_;
};
}
