#include "Reader.h"
#include "DiffView.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <chrono>
#include <cctype>

namespace cycode::reader {
namespace {
bool validId(const Json &value) { if (!value.is_string()) return false;const auto &s=value.get_ref<const std::string&>();return (s.size()==40 || s.size()==64) && std::all_of(s.begin(),s.end(),[](unsigned char c) { return (c>='0' && c<='9') || (c>='a' && c<='f'); }); }
}
void Reader::codeContext(size_t id,TextEditor::PopupData &data,bool line) {
    const auto tab=find(id);if (!tab) return;
    cyui::ScopedFont menuFont(cyui::uiFont());
    if (ImGui::IsWindowAppearing()) {
        active_=id;const auto selection=tab->editor.GetMainCursorSelection();
        if (line) {
            const auto text=tab->editor.GetLineText(data.pos.line);size_t glyphs=0;for (unsigned char c : text) if ((c&0xc0)!=0x80) ++glyphs;
            const auto end=data.pos.line+1<tab->editor.GetLineCount() ? TextEditor::DocPos(data.pos.line+1,0) : TextEditor::DocPos(data.pos.line,glyphs);
            tab->editor.SetSelections({{{data.pos.line,0},end}});
        } else if (selection.start==selection.end || data.pos<selection.start || selection.end<data.pos) tab->editor.SetCursor(data.pos);
    }
    const bool writable=!tab->readOnly(),selected=tab->editor.GetMainCursorSelection().start!=tab->editor.GetMainCursorSelection().end;
    auto item=[&](const char *label,const char *command,bool enabled=true,const char *shortcut=nullptr,Json params=nlohmann::json::object()) {
        if (ImGui::MenuItem(tr(label),shortcut,false,enabled)) { contextCommand_=command;contextParams_=std::move(params);ImGui::CloseCurrentPopup(); }
    };
    item("Undo","undo",writable && tab->canUndo(),"Ctrl+Z");item("Redo","redo",writable && tab->canRedo(),"Ctrl+Y");ImGui::Separator();
    item("Cut","cut",writable,"Ctrl+X");item("Copy","copy",true,"Ctrl+C");item("Paste","paste",writable,"Ctrl+V");item("Select all","selectAll",true,"Ctrl+A");
    item("Find selection","find",selected,"Ctrl+F");ImGui::Separator();
    item("Compare with saved version","compare.saved",!tab->untitled(),nullptr,{{"id",id}});
    item("Compare with selected tab","compare.selected",find(compareSelected_) && compareSelected_!=id,nullptr,{{"id",id}});
    for (size_t p=0;p<fileProviders_.size();++p) if (ImGui::BeginMenu(tr(fileProviders_[p].menu.c_str()))) {
        const auto &provider=fileProviders_[p];
        for (const auto &action : provider.actions) { const auto command=action.at("command").get<std::string>();item(action.at("title").get_ref<const std::string&>().c_str(),"compare.provider",providerReady(p,command),nullptr,{{"provider",p},{"command",command},{"id",id}}); }
        if (!provider.history.empty()) {
            ImGui::Separator();
            item("File history","history.open",providerReady(p,provider.history.at("file")),nullptr,{{"provider",p},{"id",id}});
            item("Selection / line history","history.selection",providerReady(p,provider.history.at("selection")),nullptr,{{"provider",p},{"id",id},{"line",data.pos.line}});
        }
        item("Refresh repository status","compare.detect",!provider.pending.valid());
        if (provider.context.contains("message")) ImGui::TextWrapped("%s",tr(provider.context.at("message").get_ref<const std::string&>().c_str()));
        ImGui::EndMenu();
    }
}
Json Reader::historyDiffParams(const std::string &command,int index) const {
    if (index<0 || static_cast<size_t>(index)>=history_.commits.size()) throw std::runtime_error("Choose a revision from history");
    const auto &commit=history_.commits.at(index);Json request;
    if (command=="history.revisions") {
        if (history_.left<0 || history_.right<0 || static_cast<size_t>(history_.left)>=history_.commits.size() || static_cast<size_t>(history_.right)>=history_.commits.size()) throw std::runtime_error("Choose two revisions from history");
        const auto &left=history_.commits.at(history_.left),&right=history_.commits.at(history_.right);
        request={{"beforeRevision",left.at("id")},{"beforePath",left.at("path")},{"afterRevision",right.at("id")},{"afterPath",right.at("path")}};
    } else if (command=="history.parent") request={{"beforeRevision",commit.at("parents").empty() ? Json("") : commit.at("parents").front()},{"beforePath",commit.at("previousPath")},{"afterRevision",commit.at("id")},{"afterPath",commit.at("path")}};
    else request={{"beforeRevision",commit.at("id")},{"beforePath",commit.at("path")}};
    if (history_.selection) { request.erase("beforePath");request.erase("afterPath"); }
    return request;
}
void Reader::pollHistoryDiff() {
    auto &diff=history_.diff;const auto tab=find(history_.document);
    const bool source=tab && pathUtf8(tab->path())==history_.path && history_.provider<fileProviders_.size();
    const bool wanted=history_.open && source && !diff.params.empty();
    const bool live=wanted && diff.params.value("afterRevision","").empty();
    const auto key=wanted ? Json{{"document",tab->id},{"path",history_.path},{"provider",history_.provider},{"generation",history_.generation},{"params",diff.params},{"revision",live ? tab->live->revision : 0},{"language",static_cast<int>(tab->codeEditor.language())}}.dump() : std::string{};
    if (diff.loadedKey!=key || !wanted) diff.ready=false;
    if (diff.pending.valid() && diff.pending.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        const auto requested=std::move(diff.pendingKey);diff.pendingKey.clear();
        try {
            const auto result=diff.pending.get();
            if (wanted && requested==key) {
                diff.loadedKey=key;
                if (result.contains("error")) throw std::runtime_error(result.at("error").get<std::string>());
                const auto &comparison=result.at("comparison");
                if (comparison.at("path")!=history_.path || comparison.at("beforeLabel").get_ref<const std::string&>().size()>4096 || comparison.at("afterLabel").get_ref<const std::string&>().size()>4096) throw std::runtime_error("Invalid file comparison result");
                auto before=normalizeEditorText(comparison.at("before").get<std::string>()),after=normalizeEditorText(comparison.at("after").get<std::string>());
                auto document=makeDiffDocument(before,after,tab->codeEditor.language());
                diff.document=std::move(document);diff.before=std::move(before);diff.after=std::move(after);diff.beforeLabel=comparison.at("beforeLabel");diff.afterLabel=comparison.at("afterLabel");
                const bool split=diff.state.split;diff.state={};diff.state.split=split;
                if (!diff.document.hunks().empty()) { diff.state.hunk=0;diff.state.focus=diff.document.hunks().front(); }
                diff.ready=true;diff.message.clear();
            }
        } catch (const std::exception &error) {
            if (wanted && requested==key) { diff.loadedKey=key;diff.ready=false;diff.message=error.what(); }
        }
    }
    if (!wanted) {
        if (history_.open && !source && !history_.path.empty()) diff.message="History document is no longer open";
        return;
    }
    if (diff.loadedKey==key) return;
    diff.message="Loading revision diff...";
    // Keep at most one worker; selection changes only replace the desired key.
    // A previous future is consumed before starting its successor, never destroyed
    // while still running on the UI thread. Obsolete replies are discarded above.
    if (!diff.pending.valid()) {
        try {
            Json input={{"path",history_.path},{"text",tab->document.text()},{"encoding",tab->document.encodingId()},{"revision",tab->live->revision}};input.update(diff.params);
            const auto &provider=fileProviders_.at(history_.provider);
            const auto call=core_.plugins().prepare(provider.plugin,provider.history.at("compare"),input);
            diff.pendingKey=key;diff.pending=std::async(std::launch::async,call);
        } catch (const std::exception &error) { diff.loadedKey=key;diff.message=error.what(); }
    }
}
void Reader::historyAction(const std::string &command,const Json &params) {
    if (command=="history.compare" || command=="history.parent" || command=="history.revisions") {
        const auto request=historyDiffParams(command,params.value("index",history_.selected));
        if (params.value("separate",false)) {
            historyComparison_=request;
            startProviderComparison(history_.provider,fileProviders_.at(history_.provider).history.at("compare"),history_.document);
        } else {
            history_.diff.params=request;history_.diff.loadedKey.clear();history_.diff.ready=false;history_.diff.message.clear();
        }
        return;
    }
    if (history_.pending.valid()) throw std::runtime_error("Wait for history to finish loading");
    const bool append=command=="history.more",refresh=command=="history.refresh";
    if (!append && !refresh) {
        const auto tab=find(params.value("id",active_));const auto provider=params.value("provider",size_t(0));
        if (!tab || tab->paged || tab->untitled() || provider>=fileProviders_.size() || fileProviders_[provider].history.empty()) throw std::runtime_error("File history provider is not available");
        sync(tab);core_.refresh();history_.document=tab->id;history_.provider=provider;history_.path=pathUtf8(tab->path());history_.selection=command=="history.selection";
        const auto selection=tab->editor.GetMainCursorSelection();history_.start=selection.start.line+1;history_.end=selection.end.line+1;
        if (selection.end.index==0 && selection.end.line>selection.start.line) --history_.end;
        if (selection.start==selection.end) history_.start=history_.end=params.value("line",tab->editor.GetMainCursorPosition().line)+1;
        history_.open=true;history_.focus=true;
    }
    const auto tab=find(history_.document);if (!tab || pathUtf8(tab->path())!=history_.path) throw std::runtime_error("History document is no longer open");
    sync(tab);core_.refresh();
    if (refresh && history_.selection) {
        const auto selection=tab->editor.GetMainCursorSelection();history_.start=selection.start.line+1;history_.end=selection.end.line+1;
        if (selection.end.index==0 && selection.end.line>selection.start.line) --history_.end;
    }
    if (append && history_.selection && history_.revision!=tab->live->revision) throw std::runtime_error("Selection changed. Refresh the history view.");
    Json cursor=append ? history_.input.value("cursor",Json::object()) : Json::object();
    if (!append) { history_.commits=Json::array();history_.selected=history_.left=history_.right=-1;++history_.generation;history_.diff.params=Json::object();history_.diff.ready=false;history_.diff.loadedKey.clear();history_.diff.message.clear(); }
    history_.revision=tab->live->revision;history_.message="Loading history...";
    history_.input={{"path",history_.path},{"text",tab->document.text()},{"encoding",tab->document.encodingId()},{"startLine",history_.start},{"endLine",history_.end},{"cursor",cursor}};
    const auto &provider=fileProviders_.at(history_.provider);
    history_.pending=std::async(std::launch::async,core_.plugins().prepare(provider.plugin,provider.history.at(history_.selection ? "selection" : "file"),history_.input));
}
void Reader::historyPanel() {
    if (history_.pending.valid() && history_.pending.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            const auto result=history_.pending.get();if (result.contains("error")) throw std::runtime_error(result.at("error").get<std::string>());
            const auto tab=find(history_.document);if (!tab || pathUtf8(tab->path())!=history_.path || (history_.selection && tab->live->revision!=history_.revision)) throw std::runtime_error("History source changed. Refresh the history view.");
            const auto &history=result.at("history");if (history.at("path")!=history_.path || !history.at("commits").is_array() || history.at("commits").size()>100) throw std::runtime_error("Invalid file history response");
            for (const auto &commit : history.at("commits")) {
                if (!validId(commit.at("id")) || !commit.at("parents").is_array() || commit.at("parents").size()>64) throw std::runtime_error("Invalid history revision");
                for (const auto &parent : commit.at("parents")) if (!validId(parent)) throw std::runtime_error("Invalid history parent");
                for (const auto *key : {"author","date","subject","path","previousPath"}) if (!commit.at(key).is_string() || commit.at(key).get_ref<const std::string&>().size()>32768) throw std::runtime_error("Invalid history metadata");
            }
            for (const auto &commit : history.at("commits")) if (std::none_of(history_.commits.begin(),history_.commits.end(),[&](const auto &c) { return c.at("id")==commit.at("id"); })) history_.commits.push_back(commit);
            history_.more=history.at("more");history_.input["cursor"]=history.at("cursor");history_.message.clear();
            if (history_.selected<0 && !history_.commits.empty()) { history_.selected=0;history_.diff.params=historyDiffParams("history.parent",0); }
        } catch (const std::exception &error) { history_.message=error.what(); }
    }
    pollHistoryDiff();
    if (!history_.open) return;
    ImGui::SetNextWindowSize(cyui::dp(1040,740),ImGuiCond_FirstUseEver);if (history_.focus) { ImGui::SetNextWindowFocus();history_.focus=false; }
    if (ImGui::Begin((std::string(tr("Git history"))+"###reader-history").c_str(),&history_.open)) {
        cyui::textDim("%s",history_.path.c_str());if (history_.selection) ImGui::Text("%s %zu–%zu",tr("Lines"),history_.start,history_.end);
        registerFilterOwner(history_.filter);cyui::FilterOptions options;options.hint="Filter history...";cyui::filterBox("##history-filter",history_.filter,options);
        { cyui::ScopedDisabled pending(history_.pending.valid());if (cyui::button(tr("Refresh history"))) execute("history.refresh");if (history_.more) { ImGui::SameLine();if (cyui::button(tr("Load older revisions"))) execute("history.more"); } }
        if (!history_.message.empty()) ImGui::TextWrapped("%s",tr(history_.message.c_str()));
        if (const auto tab=find(history_.document);!tab || pathUtf8(tab->path())!=history_.path || history_.selection && tab->live->revision!=history_.revision) cyui::callout(cyui::Tone::warning,tr("History source changed. Refresh the history view."));
        std::vector<int> visible;
        for (size_t i=0;i<history_.commits.size();++i) { const auto &c=history_.commits[i];if (history_.filter.matches(c.at("subject").get_ref<const std::string &>(),c.at("id").get<std::string>()+" "+c.at("author").get<std::string>()+" "+c.at("date").get<std::string>())) visible.push_back(static_cast<int>(i)); }
        const auto bodyHeight=ImGui::GetContentRegionAvail().y;
        if (ImGui::BeginTable("##history",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,std::max(cyui::dp(100.f),bodyHeight*history_.listFraction)})) {
            ImGui::TableSetupColumn(tr("Revision"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(96));ImGui::TableSetupColumn(tr("Date"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(160));ImGui::TableSetupColumn(tr("Author"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(140));ImGui::TableSetupColumn(tr("Message"));ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            ImGuiListClipper clip;clip.Begin(static_cast<int>(visible.size()));
            while (clip.Step()) for (int n=clip.DisplayStart;n<clip.DisplayEnd;++n) {
                const auto i=visible[n];const auto &c=history_.commits[i];ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();
                if (ImGui::Selectable((c.at("id").get<std::string>().substr(0,12)+"##revision").c_str(),history_.selected==i,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)) { history_.selected=i;execute("history.parent",{{"index",i},{"separate",ImGui::IsMouseDoubleClicked(0)}}); }
                ImGui::TableNextColumn();ImGui::TextUnformatted(c.at("date").get_ref<const std::string&>().c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(c.at("author").get_ref<const std::string&>().c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(c.at("subject").get_ref<const std::string&>().c_str());ImGui::PopID();
            }ImGui::EndTable();
        }
        if (history_.commits.empty() && !history_.pending.valid()) cyui::textDim("%s",tr("No committed history for this file or selection."));
        ImGui::InvisibleButton("##history-splitter",{ImGui::GetContentRegionAvail().x,cyui::dp(6.f)});
        const auto splitterMin=ImGui::GetItemRectMin(),splitterMax=ImGui::GetItemRectMax();const float splitterY=(splitterMin.y+splitterMax.y)*.5f;
        ImGui::GetWindowDrawList()->AddLine({splitterMin.x,splitterY},{splitterMax.x,splitterY},cyui::palette().lineStrong,cyui::dp(1.f));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive()) history_.listFraction=std::clamp(history_.listFraction+ImGui::GetIO().MouseDelta.y/std::max(1.f,bodyHeight),.15f,.6f);
        {
        cyui::ScopedDisabled disabled(history_.selected<0);
        if (cyui::button(tr("Compare with current buffer"))) execute("history.compare");ImGui::SameLine();if (cyui::button(tr("Compare with parent"))) execute("history.parent");ImGui::SameLine();
        if (cyui::button(tr("Use as left revision"))) history_.left=history_.selected;ImGui::SameLine();if (cyui::button(tr("Use as right revision"))) history_.right=history_.selected;
        const auto label=[&](int index) { return index>=0 ? history_.commits.at(index).at("id").get<std::string>().substr(0,12) : std::string(tr("Choose a revision")); };
        ImGui::Text("%s: %s  |  %s: %s",tr("Left"),label(history_.left).c_str(),tr("Right"),label(history_.right).c_str());ImGui::SameLine();
        { cyui::ScopedDisabled two(history_.left<0 || history_.right<0 || history_.left==history_.right);if (cyui::primaryButton(tr("Compare revisions"))) execute("history.revisions"); }
        }
        pollHistoryDiff();ImGui::Separator();
        auto &diff=history_.diff;
        if (diff.ready) {
            if (cyui::ghostButton(tr("Open comparison window"))) {
                try { historyComparison_=diff.params;startProviderComparison(history_.provider,fileProviders_.at(history_.provider).history.at("compare"),history_.document); }
                catch (const std::exception &error) { diff.message=error.what(); }
            }
            ImGui::SameLine();cyui::textDim("%s  |  %s",tr(diff.beforeLabel.c_str()),tr(diff.afterLabel.c_str()));
            if (!diff.message.empty()) ImGui::TextWrapped("%s",tr(diff.message.c_str()));
            if (diff.before==diff.after) cyui::textDim("%s",tr("No text differences"));
            if (diff.document.approximate()) cyui::textDim("%s",tr("Large comparison: difference grouping is approximate."));
            cyui::diffViewer("##history-diff",diff.state,diff.document);
        } else if (!diff.message.empty()) ImGui::TextWrapped("%s",tr(diff.message.c_str()));
        else if (!history_.commits.empty()) cyui::textDim("%s",tr("Select a revision to view its file diff."));
    }ImGui::End();
}
}
