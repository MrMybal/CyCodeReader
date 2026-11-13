"""Checked extensions to the single build-local native editor, never upstream CyUI."""
def augment(header, source):
    def once(text, before, after):
        if text.count(before)!=1:raise RuntimeError('Editor assistance anchor changed: '+before[:80])
        return text.replace(before,after,1)
    header=once(header,'#include <vector>','#include <vector>\n#include <map>')
    header=once(header,'\tvoid SetLanguage(const Language* language);','''\tvoid SetLanguage(const Language* language);
    // Shared ordinary-buffer assistance; hosts may replace the completion callback.
    void UseDocumentWordCompletion();
    std::vector<std::string> GetDocumentWordSuggestions(const std::string&, const Language* = nullptr);
    void SetSelectionOccurrencesEnabled(bool value) { readerOccurrencesEnabled=value; readerOccurrenceRevision=~size_t(0); }
    size_t GetSelectionOccurrenceCount() const { return readerOccurrenceRevision==transactions.getVersion() ? readerOccurrences.size() : 0; }
    void SetAllmanBlocksEnabled(bool value) { readerAllmanBlocks=value; }
''')
    header=once(header,'\tvoid renderReaderSearch();','''\tvoid renderReaderSearch();
    void updateReaderOccurrences();
    void renderReaderOccurrences();
    bool readerOccurrencesEnabled=true, readerAllmanBlocks=true;
    size_t readerWordRevision=~size_t(0), readerOccurrenceRevision=~size_t(0), readerOccurrenceCursors=0;
    DocSelection readerOccurrenceSelection;
    std::map<std::string,size_t> readerWords;
    std::vector<DocSelection> readerOccurrences;
''')
    source=once(source,'TextEditor::TextEditor() {','TextEditor::TextEditor() {\n    UseDocumentWordCompletion();')
    source=once(source,'#include "TextEditor.h"','#include "TextEditor.h"\n#include <cyui/Core.h>')
    source=once(source,'ImGui::TextUnformatted(configuration.noSuggestionsLabel.c_str());','ImGui::TextUnformatted(cyui::tr(configuration.noSuggestionsLabel.c_str()).c_str());')
    source=once(source,'void TextEditor::SetLanguage(const Language* language) {','void TextEditor::SetLanguage(const Language* language) {\n    readerWordRevision=~size_t(0);')
    source=once(source,'void TextEditor::reset() {','''void TextEditor::reset() {
    readerWordRevision=readerOccurrenceRevision=~size_t(0);
    readerWords.clear();readerOccurrences.clear();''')
    source=once(source,'\t\trenderReaderSearch();','\t\tupdateReaderOccurrences();\n\t\trenderReaderOccurrences();\n\t\trenderReaderSearch();')
    begin=source.index('void TextEditor::autoIndentAllCursors(')
    end=source.index('//\n//\tTextEditor::insertText',begin)
    source=source[:begin]+AUTOINDENT+'\n\n'+source[end:]
    begin=source.index('void TextEditor::renderReaderSearch() {')
    overlay=source[begin:];overlay=overlay[:overlay.index('\n}')+2]
    overlay=overlay.replace('renderReaderSearch()','renderReaderOccurrences()').replace('readerSearchVersion','readerOccurrenceRevision').replace('readerSearch','readerOccurrences')
    overlay=overlay.replace('ImGui::GetColorU32(readerOccurrencesColor, .28f)','ImGui::GetColorU32(palette.get(Color::selection), .55f)')
    source+='\n'+IMPLEMENTATION+'\n'+overlay+'\n'
    return header,source

AUTOINDENT=r'''void TextEditor::autoIndentAllCursors(std::shared_ptr<Transaction> transaction) {
    const auto exactInsert=[&](DocPos at,const std::string &text) {
        const bool policy=config.insertSpacesOnTabs;config.insertSpacesOnTabs=false;
        try {
            const auto end=insertText(transaction,at,text);
            config.insertSpacesOnTabs=policy;transaction->preserveTabs=true;return end;
        } catch (...) { config.insertSpacesOnTabs=policy;throw; }
    };
    for (auto cursor=cursors.begin();cursor<cursors.end();++cursor) {
        auto start=cursor->getSelectionStart();
        if (cursor->hasSelection()) {
            const auto end=cursor->getSelectionEnd();deleteText(transaction,start,end);
            cursors.adjustForDelete(cursor,start,end);
        }
        const auto blank=[](ImWchar c){return c==' ' || c=='\t';};
        // Relocate only a code block opener after a function/control header.
        // Initializer braces, comments, strings and non C-family modes are untouched.
        const auto lang=config.language ? config.language->name : std::string{};
        const bool cFamily=lang=="C" || lang=="C++" || lang=="C++ / Unreal" || lang=="C#";
        if (readerAllmanBlocks && cFamily && cursors.size()==1 && start.index &&
            document[start.line][start.index-1].codepoint=='{') {
            const auto brace=start.index-1;
            size_t lead=0,tail=brace;
            while (lead<brace && blank(document[start.line][lead].codepoint)) ++lead;
            while (tail>lead && blank(document[start.line][tail-1].codepoint)) --tail;
            const auto lexical=GetTokenColor({start.line,brace});
            bool block=tail>lead && document[start.line][tail-1].codepoint==')';
            if (!block && tail-lead<1024) {
                const auto prefix=document.getSectionText({start.line,lead},{start.line,tail});
                block=prefix=="else" || prefix=="try" || prefix=="do" ||
                    prefix.starts_with("class ") || prefix.starts_with("struct ") ||
                    prefix.starts_with("namespace ") || prefix.starts_with("enum ");
            }
            if (block && lexical!=Color::comment && lexical!=Color::string) {
                const auto indent=document.getSectionText({start.line,0},{start.line,lead});
                const DocPos from(start.line,tail);
                deleteText(transaction,from,start);cursors.adjustForDelete(cursor,from,start);
                start=exactInsert(from,"\n"+indent+"{");
                cursors.adjustForInsert(cursor,from,start);cursor->update(start,false);
            }
        }
        // Preserve the existing indentation bytes; only the new level follows
        // the user's spaces/tabs policy. The caret uses actual glyph indices.
        const auto &line=document[start.line];
        const ImWchar previous=start.index ? line[start.index-1].codepoint : 0;
        size_t after=start.index;
        while (after<line.size() && blank(line[after].codepoint)) ++after;
        const ImWchar next=after<line.size() ? line[after].codepoint : 0;
        size_t lead=0;while(lead<start.index && blank(line[lead].codepoint)) ++lead;
        const auto indent=document.getSectionText({start.line,0},{start.line,lead});
        const auto lexical=start.index ? GetTokenColor({start.line,start.index-1}) : Color::text;
        const bool open=(previous=='{' || previous=='[') && lexical!=Color::comment && lexical!=Color::string;
        const std::string unit=config.insertSpacesOnTabs ? std::string(config.tabSize,' ') : "\t";
        std::string insertion="\n"+indent+(open ? unit : "");
        const size_t caret=lead+(open ? unit.size() : 0);
        if (open && ((previous=='{' && next=='}') || (previous=='[' && next==']'))) insertion+="\n"+indent;
        if (after>start.index) { const DocPos finish(start.line,after);deleteText(transaction,start,finish);cursors.adjustForDelete(cursor,start,finish); }
        const auto end=exactInsert(start,insertion);
        cursors.adjustForInsert(cursor,start,end);cursor->update({start.line+1,caret},false);
    }
}'''

IMPLEMENTATION=r'''
void TextEditor::UseDocumentWordCompletion() {
    AutoCompleteConfig options;
    options.callback=[this](AutoCompleteState &state) {
        state.suggestions=config.readOnly ? std::vector<std::string>{} : GetDocumentWordSuggestions(state.searchTerm,state.language);
    };
    SetAutoCompleteConfig(&options);
}
std::vector<std::string> TextEditor::GetDocumentWordSuggestions(const std::string &prefix,const Language *language) {
    if (prefix.empty() || prefix.size()>256) return {};
    if (readerWordRevision!=transactions.getVersion()) {
        readerWords.clear();colorizer.update(config,document);
        size_t scanned=0;
        for (const auto &line:document) {
            for (size_t i=0;i<line.size() && scanned<4*1024*1024;) {
                const auto first=line[i].codepoint;
                if (!CodePoint::isLetter(first) && first!='_') { ++i;++scanned;continue; }
                std::string word;const auto color=line[i].color;
                while(i<line.size() && CodePoint::isWord(line[i].codepoint)) {
                    char bytes[4];const auto n=CodePoint::write(bytes,line[i++].codepoint);++scanned;
                    if (word.size()<256) word.append(bytes,n);
                }
                if (color!=Color::comment && color!=Color::string && word.size()<256 &&
                    (readerWords.size()<32768 || readerWords.contains(word))) ++readerWords[word];
            }
            if(scanned>=4*1024*1024) break;
        }
        readerWordRevision=transactions.getVersion();
    }
    auto candidates=readerWords;
    const auto selected=language ? language : config.language;
    if (selected) for (const auto *set:{&selected->keywords,&selected->declarations,&selected->identifiers})
        for(const auto &word:*set) if(candidates.size()<65536) candidates.try_emplace(word,0);
    auto fold=[](std::string value){for(auto &c:value)if(c>='A' && c<='Z')c+=char('a'-'A');return value;};
    const auto key=fold(prefix);
    std::vector<std::pair<std::string,size_t>> matches;
    for (const auto &[word,count]:candidates) if(word!=prefix && fold(word).starts_with(key)) matches.emplace_back(word,count);
    std::sort(matches.begin(),matches.end(),[&](const auto &a,const auto &b){
        const bool x=a.first.starts_with(prefix),y=b.first.starts_with(prefix);
        if(x!=y)return x;if(a.second!=b.second)return a.second>b.second;return a.first<b.first;
    });
    std::vector<std::string> result;for(const auto &entry:matches){result.push_back(entry.first);if(result.size()==128)break;}
    return result;
}
void TextEditor::updateReaderOccurrences() {
    const auto selection=GetMainCursorSelection();const auto revision=transactions.getVersion();
    if(readerOccurrenceRevision==revision && readerOccurrenceCursors==cursors.size() && selection.start==readerOccurrenceSelection.start && selection.end==readerOccurrenceSelection.end)return;
    readerOccurrenceRevision=revision;readerOccurrenceSelection=selection;readerOccurrences.clear();
    readerOccurrenceCursors=cursors.size();
    if(!readerOccurrencesEnabled || cursors.size()!=1 || selection.start.line!=selection.end.line ||
       selection.start==selection.end || selection.end.index-selection.start.index>256)return;
    const auto &selected=document[selection.start.line];
    if(selection.end.index>selected.size())return;
    for(size_t i=selection.start.index;i<selection.end.index;++i)if(!CodePoint::isWord(selected[i].codepoint))return;
    if((selection.start.index && CodePoint::isWord(selected[selection.start.index-1].codepoint)) ||
       (selection.end.index<selected.size() && CodePoint::isWord(selected[selection.end.index].codepoint)))return;
    const auto length=selection.end.index-selection.start.index;size_t scanned=0;
    for(size_t row=0;row<document.size();++row){
        const auto &line=document[row];
        for(size_t at=0;at+length<=line.size() && scanned<4*1024*1024;++at,++scanned){
            if(at && CodePoint::isWord(line[at-1].codepoint))continue;
            if(at+length<line.size() && CodePoint::isWord(line[at+length].codepoint))continue;
            size_t n=0;while(n<length && line[at+n].codepoint==selected[selection.start.index+n].codepoint)++n;
            if(n==length && !(row==selection.start.line && at==selection.start.index)){
                readerOccurrences.push_back({{row,at},{row,at+length}});if(readerOccurrences.size()==2000)return;
            }
        }
        if(scanned>=4*1024*1024)break;
    }
}
'''
