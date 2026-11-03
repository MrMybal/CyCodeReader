#include "Reader.h"
#include <cyui/Core.h>
#include <chrono>

namespace cycode::reader {
void Reader::checkUpdates() {
    if (options_.automated || releaseCheck_.valid() || updateDownload_.valid()) return;
    if (!publicUpdatesEnabled() || distribution(options_.installation)==Distribution::development) return;
    updateCancel_=false;updateMessage_="Checking for updates...";availableRelease_.reset();preparedUpdate_.reset();
    const auto mode=distribution(options_.installation);
    releaseCheck_=std::async(std::launch::async,[this,mode] { return checkForUpdate(mode,updateCancel_); });
}
void Reader::updatePanel() {
    if (!startupUpdateChecked_ && ImGui::GetTime()>2.) {
        startupUpdateChecked_=true;
        if (updateCheckOnStartup_ && !options_.automated && publicUpdatesEnabled()) checkUpdates();
    }
    if (releaseCheck_.valid() && releaseCheck_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            availableRelease_=releaseCheck_.get();updateMessage_=availableRelease_ ? "A new version is available." : "You have the latest version.";
            if (availableRelease_) updatePanel_=true;
        } catch (const std::exception &exception) { updateMessage_=exception.what(); }
    }
    if (updateDownload_.valid() && updateDownload_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try { preparedUpdate_=updateDownload_.get();updateMessage_="Update downloaded and verified."; }
        catch (const std::exception &exception) { updateMessage_=exception.what(); }
    }
    if (!updatePanel_) return;
    ImGui::SetNextWindowSize(cyui::dp(600,440),ImGuiCond_FirstUseEver);
    if (ImGui::Begin((std::string(tr("Updates"))+"###reader-updates").c_str(),&updatePanel_)) {
        cyui::heading(tr("CyCodeReader updates"),2);
        cyui::textDim("%s %s",tr("Current version:"),applicationVersion().c_str());
        const auto mode=distribution(options_.installation);
        cyui::textDim("%s",tr(mode==Distribution::portable ? "Portable edition" : mode==Distribution::installed ? "Installed edition" : "Development build"));
        if (!publicUpdatesEnabled()) cyui::callout(cyui::Tone::info,tr("Updates are prepared for the public repository. Online checks are disabled in this build."));
        else if (mode==Distribution::development) cyui::callout(cyui::Tone::info,tr("Use an installed or portable release to enable updates."));
#ifndef _WIN32
        cyui::callout(cyui::Tone::info,tr("Application updates currently require Windows x64"));
#endif
        const bool busy=releaseCheck_.valid() || updateDownload_.valid();
        {
            cyui::ScopedDisabled disabled(!publicUpdatesEnabled() || mode==Distribution::development || options_.automated);
            cyui::toggleSwitch(tr("Check for updates at startup"),&updateCheckOnStartup_);
            cyui::ScopedDisabled waiting(busy);
            if (cyui::button(tr("Check for updates"))) checkUpdates();
        }
        if (!updateMessage_.empty()) ImGui::TextWrapped("%s",tr(updateMessage_.c_str()));
        if (updateDownload_.valid() && availableRelease_) {
            const auto fraction=float(double(updateProgress_.load())/double(availableRelease_->asset.size));
            cyui::progressBar(fraction,cyui::dp(-1,0),tr("Downloading update..."));
        }
        if (busy && cyui::button(tr("Cancel download / check"))) updateCancel_=true;
        if (availableRelease_) {
            cyui::sectionTitle(availableRelease_->version.c_str());
            if (!availableRelease_->notes.empty()) {
                ImGui::BeginChild("##release-notes",cyui::dp(0,140));ImGui::TextWrapped("%s",availableRelease_->notes.c_str());ImGui::EndChild();
            }
            if (!preparedUpdate_) {
                cyui::ScopedDisabled disabled(busy);
                if (cyui::primaryButton(tr("Download update"))) {
                    updateCancel_=false;updateProgress_=0;updateMessage_="Downloading update...";
                    const auto release=*availableRelease_;const auto installation=options_.installation;
                    updateDownload_=std::async(std::launch::async,[this,release,installation] { return downloadUpdate(release,installation,updateCancel_,updateProgress_); });
                }
            } else {
                ImGui::TextWrapped("%s",tr("Reader will close normally before updating. Save prompts remain active. Close other instances using this copy."));
                cyui::ScopedDisabled disabled(dialog_ || closeId_ || overwrite_.tab);
                if (cyui::primaryButton(tr("Close and install update"))) { updateOnExit_=preparedUpdate_;requestQuit(); }
            }
        }
    }
    ImGui::End();
}
} // namespace cycode::reader
