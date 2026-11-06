#include "Reader.h"
#include "DiffView.h"
#include "cycode/Paths.h"
#include <cyui/Core.h>
#include <algorithm>
#include <chrono>

namespace cycode::reader {
namespace {
bool ready(const std::future<Json> &future) { return future.valid() && future.wait_for(std::chrono::seconds(0))==std::future_status::ready; }
}
void Reader::prepareFileProviders() {
    const auto state=core_.plugins().describe();
    for (const auto &plugin : state.at("plugins")) {
        const auto descriptor=plugin.value("fileComparisons",Json::object());if (descriptor.empty()) continue;
        if (fileProviders_.size()>=8) { serviceLogs_.append(cyui::LogLevel::warning,"File comparison provider limit reached","Plugins");break; }
        FileProvider provider;provider.plugin=plugin.at("id");provider.menu=descriptor.at("menu");provider.detect=descriptor.at("detect");provider.actions=descriptor.at("actions");
        provider.history=descriptor.value("history",Json::object());fileProviders_.push_back(std::move(provider));
    }
}
void Reader::pollFileProviders(bool force) {
    const auto tab=active();const auto path=tab && !tab->document.untitled() ? pathUtf8(tab->document.path()) : std::string{};
    for (auto &provider : fileProviders_) {
        if (ready(provider.pending)) {
            try {
                auto result=provider.pending.get();
                if (result.contains("error")) throw std::runtime_error(result.at("error").get<std::string>());
                if (!result.is_object() || !result.contains("available") || !result.at("available").is_boolean()) throw std::runtime_error("Invalid file comparison context");
                for (const auto *field : {"root","branch","message"}) if (result.contains(field) && (!result.at(field).is_string() || result.at(field).get_ref<const std::string&>().size()>32768)) throw std::runtime_error("Invalid file comparison context");
                for (const auto *field : {"tracked","conflicted","detached"}) if (result.contains(field) && !result.at(field).is_boolean()) throw std::runtime_error("Invalid file comparison context");
                if (result.at("available").get<bool>()) {
                    if (!result.contains("enabled") || !result.at("enabled").is_object()) throw std::runtime_error("Invalid file comparison context");
                    for (const auto &action : provider.actions) { const auto key=action.at("command").get<std::string>();if (result.at("enabled").contains(key) && !result.at("enabled").at(key).is_boolean()) throw std::runtime_error("Invalid file comparison context"); }
                }
                if (tab && provider.document==tab->id && provider.path==path) provider.context=std::move(result);
            } catch (const std::exception &error) { provider.context={{"available",false},{"message",error.what()}}; }
            provider.nextCheck=ImGui::GetTime()+5.;
        }
        if (provider.pending.valid()) continue;
        if (path.empty()) { provider.context={{"available",false},{"message","Save this document before using Git comparisons"}};provider.document=0;provider.path.clear();continue; }
        const bool changed=provider.document!=tab->id || provider.path!=path;
        if ((options_.automated && !force) || (!force && !changed && ImGui::GetTime()<provider.nextCheck)) continue;
        provider.document=tab->id;provider.path=path;
        if (changed) provider.context=Json::object();
        const auto call=core_.plugins().prepare(provider.plugin,provider.detect,{{"path",path}});
        provider.pending=std::async(std::launch::async,call);
    }
}
bool Reader::providerReady(size_t index,const std::string &command) const {
    const auto tab=active();if (index>=fileProviders_.size() || !tab || tab->document.untitled()) return false;
    const auto &provider=fileProviders_[index];
    if (provider.document!=tab->id || provider.path!=pathUtf8(tab->document.path()) || !provider.context.value("available",false)) return false;
    return command.empty() || provider.context.value("enabled",Json::object()).value(command,false);
}
void Reader::showComparison(std::string before,std::string after,std::string left,std::string right,size_t leftId,size_t rightId,const std::string &kind) {
    before=normalizeEditorText(before);after=normalizeEditorText(after);
    core_.refresh();
    const auto tab=find(rightId);const auto language=tab ? tab->codeEditor.language() : cyui::CodeLanguage::plain;
    auto diff=makeDiffDocument(before,after,language);
    comparison_.before=std::move(before);comparison_.after=std::move(after);comparison_.leftLabel=std::move(left);comparison_.rightLabel=std::move(right);
    comparison_.left=leftId;comparison_.right=rightId;comparison_.kind=kind;comparison_.document=std::move(diff);comparison_.ready=true;comparison_.open=true;
    comparison_.leftRevision=find(leftId) ? find(leftId)->live->revision : 0;comparison_.rightRevision=tab ? tab->live->revision : 0;
    comparison_.diskChanged=false;comparison_.nextDiskCheck=0;comparison_.message.clear();
    const auto split=comparison_.state.split;comparison_.state={};comparison_.state.split=split;
}
void Reader::compareFile(size_t id,const std::filesystem::path &path) {
    const auto tab=find(id);if (!tab) throw std::runtime_error("Comparison document is no longer open");sync(tab);
    // Read a separate snapshot, preserving both the open buffer and disk bytes.
    const auto source=Document::open(path);showComparison(source.text(),tab->document.text(),pathUtf8(source.path()),tab->document.untitled() ? tr("Untitled") : pathUtf8(tab->document.path()),0,id,"file");
    comparison_.path=pathUtf8(source.path());comparison_.fileTime=std::filesystem::last_write_time(source.path());comparison_.fileSize=std::filesystem::file_size(source.path());
}
void Reader::startProviderComparison(size_t index,const std::string &command,size_t id) {
    if (comparison_.pending.valid()) throw std::runtime_error("Wait for the current comparison to finish");
    if (index>=fileProviders_.size()) throw std::runtime_error("Comparison provider is not available");
    const auto tab=id ? find(id) : active();if (!tab || tab->document.untitled()) throw std::runtime_error("Save this document before using Git comparisons");
    const auto &provider=fileProviders_[index];
    if (std::none_of(provider.actions.begin(),provider.actions.end(),[&](const auto &a) { return a.at("command")==command; }) && provider.history.value("compare","")!=command) throw std::runtime_error("Unknown provider comparison action");
    sync(tab);core_.refresh();
    Json input={{"path",pathUtf8(tab->document.path())},{"text",tab->document.text()},{"encoding",tab->document.encodingId()},{"revision",tab->live->revision}};
    if (command==provider.history.value("compare","")) input.update(historyComparison_);
    comparison_.providerParams=command==provider.history.value("compare","") ? historyComparison_ : Json::object();
    const auto call=core_.plugins().prepare(provider.plugin,command,input);
    comparison_.kind="provider";comparison_.provider=index;comparison_.command=command;comparison_.right=tab->id;comparison_.rightRevision=tab->live->revision;comparison_.path=pathUtf8(tab->document.path());
    comparison_.ready=false;comparison_.open=true;comparison_.message="Loading comparison...";
    comparison_.pending=std::async(std::launch::async,call);
}
void Reader::compareAction(const std::string &action,const Json &params) {
    auto tab=find(params.value("id",active_));
    if (action=="compare.detect") { pollFileProviders(true);return; }
    if (action=="compare.open") {
        if (!find(compareLeft_)) compareLeft_=find(compareSelected_) ? compareSelected_ : tabs_.empty() ? 0 : tabs_.front()->id;
        if (!find(compareRight_) || compareRight_==compareLeft_) {
            compareRight_=tab && tab->id!=compareLeft_ ? tab->id : 0;
            for (const auto &candidate : tabs_) if (!compareRight_ && candidate->id!=compareLeft_) compareRight_=candidate->id;
        }
        comparison_.open=true;return;
    }
    if (comparison_.pending.valid()) throw std::runtime_error("Wait for the current comparison to finish");
    if (!tab) throw std::runtime_error("Open a document before comparing files");
    if (action=="compare.select") { compareSelected_=tab->id;compareLeft_=tab->id;cyui::notify(tr("Document selected for comparison"),cyui::Tone::info);return; }
    if (action=="compare.provider") { startProviderComparison(params.at("provider"),params.at("command"),tab->id);return; }
    sync(tab);
    if (action=="compare.file") { if (params.contains("path")) compareFile(tab->id,utf8Path(params.at("path")));else showDialog(false,tab->id,true);return; }
    if (action=="compare.saved") {
        if (tab->document.untitled()) throw std::runtime_error("Save this document before comparing its saved version");
        showComparison(tab->document.savedText(),tab->document.text(),"Last saved version",pathUtf8(tab->document.path()),0,tab->id,"saved");return;
    }
    if (action=="compare.selected" || action=="compare.tabs") {
        const auto left=find(params.value("left",compareSelected_));const auto right=find(params.value("right",tab->id));
        if (!left || !right || left==right) throw std::runtime_error("Choose two different open documents");sync(left);sync(right);
        showComparison(left->document.text(),right->document.text(),left->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(left->id) : pathUtf8(left->document.path()),right->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(right->id) : pathUtf8(right->document.path()),left->id,right->id,"tabs");
        compareLeft_=left->id;compareRight_=right->id;return;
    }
    throw std::runtime_error("Unknown comparison action");
}
bool Reader::comparisonStale() const {
    if (!comparison_.ready) return false;
    const auto left=find(comparison_.left),right=find(comparison_.right);
    return (comparison_.left && (!left || left->live->revision!=comparison_.leftRevision)) || !right || right->live->revision!=comparison_.rightRevision || comparison_.diskChanged;
}
void Reader::refreshComparison() {
    try {
        if (comparison_.kind=="tabs") compareAction("compare.tabs",{{"left",comparison_.left},{"right",comparison_.right},{"id",comparison_.right}});
        else if (comparison_.kind=="saved") compareAction("compare.saved",{{"id",comparison_.right}});
        else if (comparison_.kind=="file") compareFile(comparison_.right,utf8Path(comparison_.path));
        else if (comparison_.kind=="provider") { historyComparison_=comparison_.providerParams;startProviderComparison(comparison_.provider,comparison_.command,comparison_.right); }
    } catch (const std::exception &error) { comparison_.message=error.what(); }
}
void Reader::comparisonPanel() {
    core_.refresh();
    if (ready(comparison_.pending)) {
        try {
            const auto result=comparison_.pending.get();if (result.contains("error")) throw std::runtime_error(result.at("error").get<std::string>());
            const auto tab=find(comparison_.right);
            if (!tab || tab->live->revision!=comparison_.rightRevision || pathUtf8(tab->document.path())!=comparison_.path) throw std::runtime_error("Document changed while loading; refresh the comparison");
            const auto &diff=result.at("comparison");const auto wanted=comparison_.open;
            if (diff.at("path")!=comparison_.path || diff.at("beforeLabel").get<std::string>().size()>4096 || diff.at("afterLabel").get<std::string>().size()>4096) throw std::runtime_error("Invalid file comparison result");
            const auto before=diff.at("before").get<std::string>(),after=diff.at("after").get<std::string>();
            showComparison(before,after,diff.at("beforeLabel"),diff.at("afterLabel"),0,tab->id,"provider");
            comparison_.open=wanted;
        } catch (const std::exception &error) { comparison_.message=error.what();comparison_.ready=false; }
    }
    if (!comparison_.open) return;
    if (comparison_.kind=="file" && comparison_.ready && ImGui::GetTime()>=comparison_.nextDiskCheck) {
        comparison_.nextDiskCheck=ImGui::GetTime()+1.;std::error_code ec;
        const auto path=utf8Path(comparison_.path);const auto time=std::filesystem::last_write_time(path,ec);
        comparison_.diskChanged=bool(ec) || time!=comparison_.fileTime;
        if (!ec) { const auto size=std::filesystem::file_size(path,ec);comparison_.diskChanged|=bool(ec) || size!=comparison_.fileSize; }
    }
    ImGui::SetNextWindowSize(cyui::dp(1000,640),ImGuiCond_FirstUseEver);
    if (ImGui::Begin((std::string(tr("Comparison"))+"###reader-comparison").c_str(),&comparison_.open)) {
        if (cyui::iconButton(cyui::icon::help,tr("Context help"))) execute("help",{{"page","version-control"}});
        const auto fieldWidth=std::max(cyui::dp(100.f),(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*.5f);
        auto choose=[&](const char *label,size_t &id) {
            const auto tab=find(id);const auto preview=tab ? tab->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->document.path().filename()) : std::string(tr("Choose a document"));
            ImGui::BeginGroup();cyui::textDim("%s",tr(label));ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::BeginCombo((std::string("##")+label).c_str(),preview.c_str())) {
                for (const auto &item : tabs_) {
                    const auto name=item->document.untitled() ? std::string(tr("Untitled"))+" "+std::to_string(item->id) : pathUtf8(item->document.path());
                    ImGui::PushID(static_cast<int>(item->id));if (ImGui::Selectable(name.c_str(),item->id==id)) id=item->id;ImGui::PopID();
                }ImGui::EndCombo();
            }
            ImGui::EndGroup();
        };
        choose("Left document",compareLeft_);ImGui::SameLine();choose("Right document",compareRight_);
        {
            cyui::ScopedDisabled disabled(comparison_.pending.valid() || !find(compareLeft_) || !find(compareRight_) || compareLeft_==compareRight_);
            if (cyui::primaryButton(tr("Compare"))) execute("compare.tabs",{{"left",compareLeft_},{"right",compareRight_},{"id",compareRight_}});
        }
        if (!comparison_.message.empty()) cyui::callout(cyui::Tone::info,tr(comparison_.message.c_str()));
        if (comparison_.ready || !comparison_.kind.empty()) {
            cyui::ScopedDisabled disabled(comparison_.pending.valid());if (cyui::button(tr("Refresh comparison"))) refreshComparison();
        }
        if (comparison_.ready) {
            const bool stale=comparisonStale();
            if (stale) cyui::callout(cyui::Tone::warning,tr("Comparison snapshot is outdated. Refresh to include current changes."));
            cyui::textDim("%s  |  %s",comparison_.kind=="saved" || comparison_.kind=="provider" ? tr(comparison_.leftLabel.c_str()) : comparison_.leftLabel.c_str(),comparison_.kind=="provider" ? tr(comparison_.rightLabel.c_str()) : comparison_.rightLabel.c_str());
            if (comparison_.before==comparison_.after) cyui::textDim("%s",tr("No text differences"));
            if (comparison_.document.approximate()) cyui::callout(cyui::Tone::info,tr("Large comparison: difference grouping is approximate."));
            if (!comparison_.document.hunks().empty() && cyui::ghostButton(tr("Previous change"))) {
                auto &state=comparison_.state;const auto count=static_cast<int>(comparison_.document.hunks().size());state.hunk=state.hunk<0 ? count-1 : (state.hunk+count-1)%count;state.focus=comparison_.document.hunks()[state.hunk];
            }
            const auto request=cyui::diffViewer("##reader-diff",comparison_.state,comparison_.document);
            if (!stale && request.row>=0) {
                const auto target=request.newLine>0 ? comparison_.right : comparison_.left;const auto line=request.newLine>0 ? request.newLine : request.oldLine;
                const bool historicalRight=comparison_.providerParams.contains("afterRevision") && !comparison_.providerParams.at("afterRevision").get_ref<const std::string&>().empty();
                if (const auto tab=find(target);tab && line>0 && (comparison_.kind=="tabs" || (comparison_.kind!="provider" || comparison_.command!="staged" && !historicalRight) && request.newLine>0)) {
                    active_=tab->id;tab->editor.SetCursor({static_cast<size_t>(line-1),0});tab->focus=true;
                }
            }
        } else if (!comparison_.pending.valid() && comparison_.message.empty()) cyui::textDim("%s",tr("Choose two open documents, compare with a file, or use a repository menu."));
    }
    ImGui::End();
}
} // namespace cycode::reader
