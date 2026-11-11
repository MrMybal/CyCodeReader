#include "Reader.h"
#include "Search.h"
#include "cycode/EditorTheme.h"
#include "cycode/Paths.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <climits>

namespace cycode::reader {
namespace {
TextEditor::DocPos position(const std::string &text,size_t offset,size_t line=0,size_t column=0) {
    offset=std::min(offset,text.size());
    const auto begin=text.rfind('\n',offset ? offset-1 : std::string::npos);
    const auto start=offset==0 || begin==std::string::npos ? 0 : begin+1;
    if (!line) line=1+std::count(text.begin(),text.begin()+offset,'\n');
    size_t glyph=0;for (size_t i=start;i<offset;++i) if ((static_cast<unsigned char>(text[i])&0xc0)!=0x80) ++glyph;
    return {line-1,glyph};
}
size_t offsetAt(const std::string &text,TextEditor::DocPos pos) {
    size_t at=0;for (size_t l=0;l<pos.line && at<text.size();++l) { const auto n=text.find('\n',at);at=n==std::string::npos ? text.size() : n+1; }
    for (size_t i=0;i<pos.index && at<text.size() && text[at]!='\n';++i) { ++at;while (at<text.size() && (static_cast<unsigned char>(text[at])&0xc0)==0x80) ++at; }
    return at;
}
}
void Reader::openFind(bool replace,bool all) {
    if (const auto tab=active()) {
        sync(tab);const auto selected=tab->paged ? std::string() : tab->editor.GetCursorText(0);
        if (!selected.empty() && selected.size()<=4096 && selected.find('\n')==std::string::npos) searchQuery_=selected;
        find_.document=tab->id;
    }
    find_.replace=replace;find_.focus=!all;find_.open=!all;searchPanel_=all;searchFocus_=all;
    find_.selection=false;find_.key.clear();find_.message.clear();
}
void Reader::rebuildSearch() {
    for (const auto &tab : tabs_) sync(tab);core_.refresh();
    std::string key=Json{{"query",searchQuery_},{"case",find_.sensitive},{"words",find_.words},{"extended",find_.extended},{"selection",find_.selection},{"begin",find_.begin},{"end",find_.end},{"document",active_},{"all",searchPanel_}}.dump();
    const auto pagedQuery=expandedSearchText(searchQuery_,find_.extended);
    for (const auto &tab:tabs_) if (tab->paged && (searchPanel_ || tab->id==active_)) {
        tab->paged->search(pagedQuery,find_.sensitive,find_.words);
        key+="/paged/"+std::to_string(tab->paged->status().searchGeneration);
    }
    for (const auto &tab : tabs_) key+="/"+std::to_string(tab->id)+":"+std::to_string(tab->live->revision);
    if (key==find_.key) return;find_.key=key;find_.message.clear();
    searchMatches_={{"matches",Json::array()},{"truncated",false}};searchRows_.clear();searchNavigation_.clear();++searchRowsRevision_;
    const auto query=normalizeEditorText(expandedSearchText(searchQuery_,find_.extended));
    if (query.empty()) return;
    if (query.size()>4096) { find_.message="Search query must contain 1 to 4096 bytes";return; }
    for (const auto &tab : tabs_) {
        if (!searchPanel_ && tab->id!=active_) continue;
        if (tab->paged) {
            const auto &status=tab->paged->status();
            if (status.searching) find_.message="Searching paged text...";
            if (status.searchedRevision==status.revision) for (const auto &m:status.matches) {
                if (searchRows_.size()==2000) { searchMatches_["truncated"]=true;break; }
                const auto id=uiId("paged-search/"+std::to_string(tab->id)+"/"+std::to_string(status.revision)+"/"+std::to_string(m.offset));
                Json match={{"id",tab->id},{"paged",true},{"revision",status.revision},{"offset",m.offset},{"length",m.length},
                    {"line",m.range.first().line+1},{"column",m.range.first().character+1},
                    {"endLine",m.range.last().line},{"endCharacter",m.range.last().character}};
                // Diagnostics only have int labels. Navigation retains the full 64-bit positions.
                searchRows_.push_back({id,cyui::LogLevel::info,pathUtf8(tab->path()),m.preview,
                    static_cast<int>(std::min<uint64_t>(m.range.first().line+1,INT_MAX)),
                    static_cast<int>(std::min<uint64_t>(m.range.first().character+1,INT_MAX))});
                searchNavigation_[id]=match;searchMatches_["matches"].push_back(std::move(match));
            }
            if (status.truncated) searchMatches_["truncated"]=true;
            continue;
        }
        size_t begin=0,end=std::string::npos;
        if (!searchPanel_ && find_.selection) {
            if (find_.document!=tab->id || find_.selectionRevision!=tab->live->revision) { find_.message="Selection changed. Select the search range again.";continue; }
            begin=find_.begin;end=find_.end;
        }
        const auto found=findTextMatches(tab->document.text(),query,find_.sensitive,find_.words,2000-searchRows_.size(),begin,end);
        for (const auto &m : found.matches) {
            Json match={{"id",tab->id},{"revision",tab->live->revision},{"offset",m.offset},{"length",m.length},{"line",m.line},{"column",m.column}};
            const auto id=uiId("search/"+std::to_string(tab->id)+"/"+std::to_string(tab->live->revision)+"/"+std::to_string(m.offset));
            const auto source=tab->untitled() ? std::string(tr("Untitled"))+" "+std::to_string(tab->id) : pathUtf8(tab->path());
            const auto start=m.offset-m.column+1,stop=tab->document.text().find('\n',m.offset);
            const auto snippet=tab->document.text().substr(start,std::min(size_t(512),(stop==std::string::npos ? tab->document.text().size() : stop)-start));
            searchRows_.push_back({id,cyui::LogLevel::info,source,snippet,static_cast<int>(m.line),static_cast<int>(m.column)});searchNavigation_[id]=match;searchMatches_["matches"].push_back(match);
        }
        if (found.truncated) { searchMatches_["truncated"]=true;break; }
    }
}
void Reader::navigateSearch(const Json &match,bool focus) {
    const auto tab=find(match.at("id"));if (!tab || tab->live->revision!=match.at("revision")) { find_.key.clear();return; }
    if (tab->paged) {
        cyui::PagedCodePosition first{match.at("line").get<uint64_t>()-1,match.at("column").get<uint64_t>()-1};
        cyui::PagedCodePosition last{match.at("endLine").get<uint64_t>(),match.at("endCharacter").get<uint64_t>()};
        tab->paged->view().setSelection({first,last});active_=tab->id;tab->focus=focus;return;
    }
    const auto &text=tab->document.text();const auto at=match.at("offset").get<size_t>(),length=match.at("length").get<size_t>();
    if (at>text.size() || length>text.size()-at) return;
    const auto start=position(text,at,match.at("line"));const auto end=position(text,at+length);
    active_=tab->id;tab->editor.SetSelections({{start,end}});tab->editor.ScrollToLine(start.line);tab->focus=focus;
}
void Reader::findMatch(bool previous) {
    if (searchQuery_.empty()) { openFind();return; }
    // Local navigation always targets the current editor, even with global results open.
    const auto tab=active();if (!tab) return;sync(tab);core_.refresh();
    const auto query=tab->paged ? expandedSearchText(searchQuery_,find_.extended) : normalizeEditorText(expandedSearchText(searchQuery_,find_.extended));
    if (query.size()>4096) { find_.message="Search query must contain 1 to 4096 bytes";return; }
    if (tab->paged) { tab->paged->search(query,find_.sensitive,find_.words);tab->paged->find(previous,find_.wrap);return; }
    size_t begin=0,end=std::string::npos;
    if (find_.selection) { if (find_.document!=tab->id || find_.selectionRevision!=tab->live->revision) { find_.message="Selection changed. Select the search range again.";return; }begin=find_.begin;end=find_.end; }
    const auto found=findTextMatches(tab->document.text(),query,find_.sensitive,find_.words,2000,begin,end);
    if (found.matches.empty()) { find_.message="No matches";return; }
    const auto selection=tab->editor.GetMainCursorSelection();const auto offset=offsetAt(tab->document.text(),previous ? selection.start : selection.end);
    const TextMatch *match=nullptr;
    if (previous) { for (const auto &m : found.matches) if (m.offset<offset) match=&m; }
    else for (const auto &m : found.matches) if (m.offset>=offset) { match=&m;break; }
    if (!match && find_.wrap) match=previous ? &found.matches.back() : &found.matches.front();
    if (!match) { find_.message=previous ? "Start of document reached" : "End of document reached";return; }
    navigateSearch({{"id",tab->id},{"revision",tab->live->revision},{"offset",match->offset},{"length",match->length},{"line",match->line}},!find_.open);
    find_.message.clear();
}
void Reader::replaceMatches(bool all) {
    const auto tab=active();if (!tab || tab->readOnly()) return;sync(tab);core_.refresh();
    const auto query=tab->paged ? expandedSearchText(searchQuery_,find_.extended) : normalizeEditorText(expandedSearchText(searchQuery_,find_.extended));if (query.empty()) return;
    if (query.size()>4096) { find_.message="Search query must contain 1 to 4096 bytes";return; }
    const auto replacement=normalizeEditorText(expandedSearchText(find_.replacement,find_.extended));
    if (tab->paged) {
        if (all) { find_.message="Replace all is unavailable in paged mode.";return; }
        tab->paged->search(query,find_.sensitive,find_.words);
        const auto &status=tab->paged->status();const auto selection=tab->paged->view().selection();
        const auto hit=std::find_if(status.matches.begin(),status.matches.end(),[&](const auto &m) {
            return m.range.anchor==selection.first() && m.range.caret==selection.last();
        });
        if (status.searchedRevision!=status.revision || hit==status.matches.end()) { tab->paged->find(false,find_.wrap);return; }
        tab->paged->replace(replacement);find_.key.clear();return;
    }
    const auto &text=tab->document.text();size_t begin=0,end=text.size();
    if (find_.selection) { if (find_.document!=tab->id || find_.selectionRevision!=tab->live->revision) { find_.message="Selection changed. Select the search range again.";return; }begin=find_.begin;end=find_.end; }
    const auto found=findTextMatches(text,query,find_.sensitive,find_.words,2000,begin,end);
    if (found.truncated && all) { find_.message="Too many replacements. Narrow the search or use a selection.";return; }
    if (!all) {
        const auto selection=tab->editor.GetMainCursorSelection();const auto start=offsetAt(text,selection.start),stop=offsetAt(text,selection.end);
        const bool selected=std::any_of(found.matches.begin(),found.matches.end(),[&](const auto &m) { return m.offset==start && m.offset+m.length==stop; });
        if (!selected) { findMatch();return; }
        auto next=text;next.replace(start,stop-start,replacement);validateEditorText(next);
        tab->editor.ReplaceTextPreservingTabs(replacement);sync(tab);
        if (find_.selection) { find_.end=find_.end-(stop-start)+replacement.size();find_.selectionRevision=tab->live->revision; }
        findMatch();
    } else {
        if (found.matches.empty()) { find_.message="No matches";return; }
        auto next=text;for (auto i=found.matches.rbegin();i!=found.matches.rend();++i) next.replace(i->offset,i->length,replacement);validateEditorText(next);
        tab->editor.SelectAll();tab->editor.ReplaceTextPreservingTabs(next);sync(tab);
        if (find_.selection) { find_.end=end;for (const auto &m : found.matches) find_.end=find_.end-m.length+replacement.size();find_.selectionRevision=tab->live->revision; }
        find_.message=std::to_string(found.matches.size())+" "+tr("replacements");
    }
    find_.key.clear();
}
void Reader::updateSearchMarkers(const std::shared_ptr<Tab> &tab) {
    if (tab->paged) return;
    const bool inSelection=find_.selection && !searchPanel_;
    const auto key=searchQuery_+"/"+std::to_string(searchPanel_)+"/"+std::to_string(tab->editor.GetTextRevision())+"/"+std::to_string(tab->live->revision)+"/"+std::to_string(find_.sensitive)+std::to_string(find_.words)+std::to_string(find_.extended)+"/"+std::to_string(cyui::palette().warning)+"/"+std::to_string(find_.selection)+":"+std::to_string(find_.document)+":"+std::to_string(find_.selectionRevision)+":"+std::to_string(find_.begin)+":"+std::to_string(find_.end);
    if (tab->searchMarkerKey==key) return;tab->searchMarkerKey=key;
    const auto &text=tab->document.text();const auto query=normalizeEditorText(expandedSearchText(searchQuery_,find_.extended));
    if (query.size()>4096 || (inSelection && (find_.document!=tab->id || find_.selectionRevision!=tab->live->revision))) { tab->editor.SetReaderSearchMatches({},cyui::palette().warning);return; }
    const auto found=findTextMatches(text,query,find_.sensitive,find_.words,2000,inSelection ? find_.begin : 0,inSelection ? find_.end : std::string::npos);
    std::vector<TextEditor::DocSelection> ranges;
    for (const auto &m : found.matches) {
        const auto start=position(text,m.offset,m.line);auto finish=start;
        for (size_t i=m.offset;i<m.offset+m.length;++i) { if (text[i]=='\n') { ++finish.line;finish.index=0; } else if ((static_cast<unsigned char>(text[i])&0xc0)!=0x80) ++finish.index; }
        ranges.push_back({start,finish});
    }
    tab->editor.SetReaderSearchMatches(std::move(ranges),cyui::palette().warning);
}
void Reader::searchWindows() {
    if (!find_.open && !searchPanel_) return;
    if (find_.open) {
        ImGui::SetNextWindowSize(cyui::dp(640,300),ImGuiCond_FirstUseEver);
        if (find_.focus) ImGui::SetNextWindowFocus();
        if (ImGui::Begin((std::string(tr("Find / replace"))+"###reader-find-replace").c_str(),&find_.open,ImGuiWindowFlags_NoCollapse)) {
        if (cyui::iconButton(cyui::icon::help,tr("Context help"))) execute("help",{{"page","search"}});
            int mode=find_.replace ? 1 : 0;const char *modes[]={tr("Find"),tr("Replace")};
            cyui::segmented("##find-tabs",modes,2,&mode,cyui::dp(220));find_.replace=mode==1;
            bool enter=false;
            if (cyui::beginProperties("##find-fields",110)) {
                cyui::property(tr("Search text"));if (find_.focus) { ImGui::SetKeyboardFocusHere();find_.focus=false; }
                enter=ImGui::InputText("##find-query",&searchQuery_,ImGuiInputTextFlags_EnterReturnsTrue);
                if (find_.replace) { cyui::property(tr("Replace with"));ImGui::InputText("##find-replacement",&find_.replacement); }
                cyui::endProperties();
            }
            ImGui::Checkbox(tr("Match case"),&find_.sensitive);ImGui::SameLine();ImGui::Checkbox(tr("Whole word"),&find_.words);ImGui::SameLine();ImGui::Checkbox(tr("Wrap around"),&find_.wrap);
            ImGui::Checkbox(tr("Extended escapes"),&find_.extended);cyui::tooltip(tr("Interpret \\n, \\r, \\t and \\\\."));ImGui::SameLine();
            bool selection=find_.selection;
            ImGui::BeginDisabled(active() && active()->paged);
            if (ImGui::Checkbox(tr("In selection"),&selection)) {
                if (auto tab=active();tab && !tab->paged) { sync(tab);core_.refresh();const auto s=tab->editor.GetMainCursorSelection();find_.begin=offsetAt(tab->document.text(),s.start);find_.end=offsetAt(tab->document.text(),s.end);find_.selection=selection && find_.end>find_.begin;find_.document=tab->id;find_.selectionRevision=tab->live->revision; }
            }
            ImGui::EndDisabled();
            cyui::ScopedDisabled disabled(searchQuery_.empty() || !active());
            if (cyui::primaryButton(tr("Find next")) || enter) findMatch();ImGui::SameLine();if (cyui::button(tr("Find previous"))) findMatch(true);
            if (find_.replace) { cyui::ScopedDisabled ro(!active() || active()->readOnly());ImGui::SameLine();if (cyui::button(tr("Replace"))) replaceMatches(false);ImGui::SameLine();if (cyui::button(tr("Replace all"))) replaceMatches(true); }
            if (cyui::button(tr("Find in all open documents"))) { searchPanel_=true;searchFocus_=true;find_.open=false; }
            ImGui::SameLine();if (cyui::button(tr("Clear search marks"))) { searchQuery_.clear();find_.key.clear(); }
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape)) find_.open=false;
            rebuildSearch();ImGui::Text("%zu %s",searchRows_.size(),tr("matches"));
            if (!find_.message.empty()) ImGui::TextWrapped("%s",tr(find_.message.c_str()));
            if (searchMatches_.value("truncated",false)) cyui::textDim("%s",tr("Search results limited to 2000 matches."));
        }ImGui::End();
    }
    if (searchPanel_) {
        ImGui::SetNextWindowSize(cyui::dp(1000,660),ImGuiCond_FirstUseEver);
        if (searchFocus_) ImGui::SetNextWindowFocus();
        if (ImGui::Begin((std::string(tr("Search open documents"))+"###reader-search").c_str(),&searchPanel_)) {
        if (cyui::iconButton(cyui::icon::help,tr("Context help"))) execute("help",{{"page","search"}});
            if (searchFocus_) { ImGui::SetKeyboardFocusHere();searchFocus_=false; }
            ImGui::SetNextItemWidth(std::max(cyui::dp(120.f),ImGui::GetContentRegionAvail().x-cyui::dp(300.f)));ImGui::InputTextWithHint("##query",tr("Search text"),&searchQuery_);ImGui::SameLine();
            ImGui::Checkbox(tr("Match case"),&find_.sensitive);ImGui::SameLine();ImGui::Checkbox(tr("Whole word"),&find_.words);
            rebuildSearch();
            ImGui::Text("%zu %s",searchRows_.size(),tr("matches in open documents"));
            if (!find_.message.empty()) cyui::textDim("%s",tr(find_.message.c_str()));
            if (searchMatches_.value("truncated",false)) cyui::callout(cyui::Tone::warning,tr("Search results limited to 2000 matches."));
            registerFilterOwner(searchResultsFilter_);cyui::FilterOptions field;field.hint="Filter search results...";cyui::filterBox("##result-filter",searchResultsFilter_,field);
            const auto &rows=searchFilteredRows_.rows(searchRows_,searchRowsRevision_,searchResultsFilter_);
            ImGui::BeginChild("##matches",{0,std::max(cyui::dp(100.f),ImGui::GetContentRegionAvail().y*.48f)},ImGuiChildFlags_Borders);
            ImGuiListClipper clip;clip.Begin(static_cast<int>(rows.size()),ImGui::GetTextLineHeightWithSpacing());
            while (clip.Step()) for (int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                const auto &row=rows[i];ImGui::PushID(static_cast<int>(row.id));
                const auto y=ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##hit",selectedSearch_==row.id,ImGuiSelectableFlags_AllowDoubleClick)) { selectedSearch_=row.id;navigateSearch(searchNavigation_.at(row.id),ImGui::IsMouseDoubleClicked(0)); }
                auto *d=ImGui::GetWindowDrawList();const auto suffix=std::filesystem::path(row.source).filename().string()+":"+std::to_string(row.line);
                const auto suffixWidth=ImGui::CalcTextSize(suffix.c_str()).x;const auto width=ImGui::GetContentRegionAvail().x-suffixWidth-cyui::dp(20.f);
                d->PushClipRect(y,{y.x+std::max(0.f,width),y.y+ImGui::GetTextLineHeight()},true);
                const auto query=normalizeEditorText(expandedSearchText(searchQuery_,find_.extended));const auto highlights=findTextMatches(row.message,query,find_.sensitive,find_.words,100);
                for (const auto &m : highlights.matches) { const auto a=ImGui::CalcTextSize(row.message.data(),row.message.data()+m.offset).x,b=ImGui::CalcTextSize(row.message.data(),row.message.data()+m.offset+m.length).x;d->AddRectFilled({y.x+a,y.y},{y.x+b,y.y+ImGui::GetTextLineHeight()},cyui::alpha(cyui::palette().warning,.4f)); }
                d->AddText(y,cyui::palette().text,row.message.c_str());d->PopClipRect();
                d->AddText({y.x+std::max(0.f,width)+cyui::dp(12.f),y.y},cyui::palette().textDim,suffix.c_str());ImGui::PopID();
            }ImGui::EndChild();
            if (!searchNavigation_.contains(selectedSearch_) && !rows.empty()) selectedSearch_=rows.front().id;
            if (searchNavigation_.contains(selectedSearch_)) {
                const auto &match=searchNavigation_.at(selectedSearch_);const auto tab=find(match.at("id"));
                if (tab && tab->paged && match.at("revision")==tab->live->revision) {
                    if (cyui::button(tr("Open result in paged source"))) navigateSearch(match);
                    cyui::textDim("%s",tr("Paged results open in the source view; no full-text preview copy."));
                } else if (tab && match.at("revision")==tab->live->revision) {
                    const auto key=match.dump()+find_.key+"/"+std::to_string(cyui::palette().warning)+"/"+std::to_string(static_cast<int>(tab->codeEditor.language()));
                    if (key!=searchPreviewKey_) {
                        searchPreviewKey_=key;
                        configureCodeEditor(searchPreview_,editorFont_);
                        searchPreview_.setLanguage(tab->codeEditor.language());
                        searchPreview_.setText(tab->document.text());
                        searchPreview_.setReadOnly(true);
                        const auto p=position(tab->document.text(),match.at("offset"),match.at("line"));
                        searchPreview_.native().SetCursor(p);
                        searchPreview_.native().ScrollToLine(p.line>5 ? p.line-5 : 0);
                        searchPreview_.native().SetFindReplaceEnabled(false);
                        // Same UTF-8/glyph conversion as live tabs; query/options/theme
                        // invalidate the marks even when the selected offset stays put.
                        const auto &text=tab->document.text();
                        const auto query=normalizeEditorText(expandedSearchText(searchQuery_,find_.extended));
                        std::vector<TextEditor::DocSelection> ranges;
                        if (query.size()<=4096) for (const auto &m : findTextMatches(text,query,find_.sensitive,find_.words,2000).matches)
                            ranges.push_back({position(text,m.offset,m.line),position(text,m.offset+m.length)});
                        searchPreview_.native().SetReaderSearchMatches(std::move(ranges),cyui::palette().warning);
                    }
                    if (cyui::button(tr("Open result"))) navigateSearch(match);ImGui::SameLine();cyui::textDim("%s",tab->untitled() ? tr("Untitled") : pathUtf8(tab->path()).c_str());
                    searchPreview_.draw("##search-preview");
                }
            } else cyui::textDim("%s",searchQuery_.empty() ? tr("Type to search all open documents, including unsaved buffers.") : tr("No matches"));
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape)) searchPanel_=false;
        }ImGui::End();
    }
}
}
