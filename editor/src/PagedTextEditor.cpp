#include "cycode/PagedTextEditor.h"
#include <cycode/paged/LargeText.h>
#include <cycode/text/Search.h>
#include <cycode/TextFile.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace cycode {
namespace {
constexpr std::size_t chunkBytes=65536, clipboardBudget=4*1024*1024, syntaxBudget=32*1024*1024;
using Position=cyui::PagedCodePosition;
using Selection=cyui::PagedCodeSelection;
using Command=cyui::PagedCodeCommand;
struct Cursor {
    Position position{};
    bool cr=false;
    void advance(std::string_view text) {
        for (std::size_t at=0;at<text.size();) {
            const auto c=static_cast<unsigned char>(text[at]);
            if (c=='\r') { ++position.line;position.character=0;cr=true;++at; }
            else if (c=='\n') { if (!cr) ++position.line;position.character=0;cr=false;++at; }
            else { const auto width=c<0x80 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
                position.character+=width==4 ? 2 : 1;at+=width;cr=false; }
        }
    }
};
std::string insertion(std::string_view text,const std::string &newline) {
    std::string value;value.reserve(text.size());
    for (std::size_t i=0;i<text.size();++i) {
        if (text[i]=='\r' || text[i]=='\n') {
            if (text[i]=='\r' && i+1<text.size() && text[i+1]=='\n') ++i;
            value+=newline;
        } else value+=text[i];
        if (value.size()>1024*1024) throw std::runtime_error("Insertion exceeds the 1 MiB operation budget");
    }
    return value;
}
bool writable(const std::filesystem::path &path) {
    const auto permissions=std::filesystem::status(path).permissions();
    using P=std::filesystem::perms;
    return (permissions&(P::owner_write|P::group_write|P::others_write))!=P::none;
}
}
struct PagedTextEditor::Impl {
    struct History { LargeTextSnapshot before,after;Selection previous,next; };
    struct Completion {
        enum Kind { imported,page,edited,changed,saved,copied,searched,presentation,failed } kind=failed;
        std::uint64_t request=0, expected=0, search=0;
        Status status;
        cyui::PagedCodePage value;
        Selection selection;
        std::string text,error;
        bool cut=false;
    };
    cyui::PagedCodeEditor view;
    Status ui;
    const cyui::ItemId identity;
    std::vector<cyui::PagedCodeInput> cancelled;
    std::vector<cyui::PagedCodeReplacement> rejected;
    std::deque<std::pair<int,std::function<void()>>> jobs;
    std::deque<Completion> completed;
    std::mutex mutex;
    std::condition_variable wake;
    bool stopping=false;
    std::thread worker;
    std::atomic<std::uint64_t> readEpoch{0},searchEpoch{0};
    std::uint64_t activeRead=0, activeEdit=0;
    bool actionPending=false, queuedInput=false, waitingForView=false;
    cyui::CodeLanguage language=cyui::CodeLanguage::plain;
    std::string searchKey;
    std::optional<bool> navigateAfterSearch;
    // Worker-exclusive native state; never inspected by drawing or the UI thread.
    std::shared_ptr<SpoolStore> store;
    std::unique_ptr<LargeText> native;
    Status nativeStatus;
    std::shared_ptr<PagedTextBackend> backend;
    std::vector<History> history;
    std::size_t historyIndex=0;
    LargeTextSnapshot coloured;
    cyui::CodeLanguage colouredLanguage=cyui::CodeLanguage::plain;
    std::uint64_t catalogue=0;
    std::vector<cyui::CodeToken> tokens;
    std::vector<Match> marks;
    std::uint64_t markedRevision=0;
    Impl(cyui::ItemId id,const std::filesystem::path &source,const std::filesystem::path &work):identity(id) {
        if (!id) throw std::invalid_argument("Paged document identity must be nonzero");
        ui.path=source;ui.message="Importing and indexing UTF-8 text...";
        start();
        enqueue([this,source,work] {
            Completion c;c.kind=Completion::imported;
            try {
                std::filesystem::create_directories(work);
                store=std::make_shared<SpoolStore>(work);
                native=std::make_unique<LargeText>(source,store);
                nativeStatus.path=source;nativeStatus.revision=1;nativeStatus.ready=true;
                nativeStatus.readOnly=!writable(source);
                for (std::uint64_t offset=0;offset<native->bytes();) {
                    const auto prefix=native->read(offset,chunkBytes);
                    if (const auto at=prefix.find_first_of("\r\n");at!=std::string::npos) {
                        if (prefix[at]=='\n') nativeStatus.newline="\n";
                        else {
                            const bool lf=at+1<prefix.size() ? prefix[at+1]=='\n' :
                                offset+prefix.size()<native->bytes() && native->read(offset+prefix.size(),4).starts_with("\n");
                            nativeStatus.newline=lf ? "\r\n" : "\r";
                        }
                        break;
                    }
                    offset+=prefix.size();
                }
                refresh();c.status=nativeStatus;
            } catch (const std::exception &e) { c.error=e.what(); }
            post(std::move(c));
        });
    }
    Impl(cyui::ItemId id,std::shared_ptr<PagedTextBackend> supplied):identity(id),backend(std::move(supplied)) {
        if (!id || !backend) throw std::invalid_argument("A nonzero native identity and backend are required");
        ui.message="Loading native paged backend...";start();
        enqueue([this] {
            Completion c;c.kind=Completion::imported;
            try { adopt(backend->state(identity));c.status=nativeStatus; }
            catch (const std::exception &e) { c.error=e.what(); }post(std::move(c));
        });
    }
    void start() {
        worker=std::thread([this] {
            for (;;) {
                std::function<void()> job;
                { std::unique_lock lock(mutex);wake.wait(lock,[&] { return stopping || !jobs.empty(); });
                    if (stopping && jobs.empty()) return;job=std::move(jobs.front().second);jobs.pop_front(); }
                job();
            }
        });
    }
    void adopt(Status value) {
        if (!value.ready || !value.revision || !value.lines ||
            (value.newline!="\n" && value.newline!="\r" && value.newline!="\r\n"))
            throw std::runtime_error("Invalid native paged backend state");
        value.busy=false;nativeStatus=std::move(value);
    }
    std::string readBytes(std::uint64_t at,std::size_t count) {
        if (!backend) return native->read(at,count);
        auto part=backend->readBytes(identity,nativeStatus.revision,at,count);
        if (at>nativeStatus.bytes || part.text.size()>count || part.text.size()>nativeStatus.bytes-at ||
            part.nextOffset!=at+part.text.size() || (part.text.empty() && at!=nativeStatus.bytes))
            throw std::runtime_error("Invalid native UTF-8 byte continuation");
        return std::move(part.text);
    }
    ~Impl() {
        ++readEpoch;++searchEpoch;
        { std::lock_guard lock(mutex);stopping=true;jobs.clear(); }
        wake.notify_one();if (worker.joinable()) worker.join();
    }
    void enqueue(std::function<void()> job,int coalesce=0) {
        { std::lock_guard lock(mutex);if (coalesce) std::erase_if(jobs,[&](const auto &j) { return j.first==coalesce; });jobs.emplace_back(coalesce,std::move(job)); }wake.notify_one();
    }
    void post(Completion value) { std::lock_guard lock(mutex);completed.push_back(std::move(value)); }
    cyui::PagedCodeDocument document() const { return {identity,ui.revision,ui.lines,0,ui.readOnly}; }
    void refresh() {
        nativeStatus.lines=native->readLines(0,0,4).totalLines; // native lazy index, worker only
        nativeStatus.bytes=native->bytes();nativeStatus.dirty=native->dirty();nativeStatus.busy=false;
        nativeStatus.canUndo=historyIndex>0;nativeStatus.canRedo=historyIndex<history.size();
    }
    void guard(std::uint64_t expected,bool modifying=false) {
        if ((!native && !backend) || expected!=nativeStatus.revision) throw std::runtime_error("Stale paged document revision");
        if (modifying && nativeStatus.revision==UINT64_MAX) throw std::runtime_error("Paged revision counter exhausted");
        if (modifying && nativeStatus.readOnly) throw std::runtime_error("Paged document is read-only");
    }
    std::uint64_t offset(Position p) { return backend ? backend->offset(identity,nativeStatus.revision,p) : native->readLines(p.line,p.character,4).offset; }
    void prepareSyntax(cyui::CodeLanguage selected) {
        if (backend) return;
        const auto revision=cyui::codeLanguageRevision();
        if (coloured==native->snapshot() && colouredLanguage==selected && catalogue==revision) return;
        tokens.clear();coloured.reset();nativeStatus.syntaxMessage.clear();
        if (selected!=cyui::CodeLanguage::plain) {
            if (native->bytes()>syntaxBudget)
                nativeStatus.syntaxMessage="Syntax highlighting is unavailable above 32 MiB.";
            else {
                std::string full;full.reserve(static_cast<std::size_t>(native->bytes()));
                for (std::uint64_t at=0;at<native->bytes();) { const auto part=native->read(at,chunkBytes);full+=part;at+=part.size(); }
                // Whole snapshot supplies lexical context, including initial BOM and all preceding lines.
                try { tokens=cyui::highlightCode(full,selected); }
                catch (const std::exception &e) { nativeStatus.syntaxMessage=e.what(); }
            }
        }
        coloured=native->snapshot();colouredLanguage=selected;catalogue=revision;
    }
    cyui::PagedCodePage page(const cyui::PagedCodeRead &r,std::uint64_t epoch,
                             cyui::CodeLanguage selected,std::uint64_t firstColumn,bool allowReveal=true) {
        guard(r.revision);prepareSyntax(selected);
        cyui::PagedCodePage p{r.id,r.document,r.revision,firstColumn,{}};
        if (r.document!=identity || r.firstLine>=nativeStatus.lines || !r.lines || r.lines>4096 || !r.columns || r.columns>8192 || !r.maxBytes || r.maxBytes>4*1024*1024)
            throw std::runtime_error("Invalid paged read budget");
        auto at=offset({r.firstLine,0});
        const auto until=r.firstLine+std::min<std::uint64_t>(r.lines,nativeStatus.lines-r.firstLine);
        std::size_t bytes=0,tokenCount=0;
        std::string buffer;std::size_t index=0;
        auto fill=[&] {
            if (index<buffer.size()) return true;
            if (at==nativeStatus.bytes) return false;
            buffer=readBytes(at,chunkBytes);index=0;
            return !buffer.empty();
        };
        for (auto line=r.firstLine;line<until;++line) {
            const auto start=at;
            const auto reveal=r.reveal && r.reveal->line==line ? std::optional(r.reveal->character) : std::nullopt;
            cyui::PagedCodeLineBuilder builder(line,firstColumn,r.columns,r.tabSize,reveal);
            while (fill()) {
                if (epoch!=readEpoch) throw std::runtime_error("Paged read cancelled");
                const auto end=buffer.find_first_of("\r\n",index);
                const auto stop=end==std::string::npos ? buffer.size() : end;
                if (!builder.append(std::string_view(buffer).substr(index,stop-index))) throw std::runtime_error("Cannot prepare UTF-8 line fragment");
                at+=stop-index;index=stop;
                if (end!=std::string::npos) {
                    const bool cr=buffer[index]=='\r';++index;++at;
                    if (cr && fill() && buffer[index]=='\n') { ++index;++at; }
                    break;
                }
            }
            cyui::PagedCodeLine fragment;std::string error;
            if (!builder.finish(fragment,&error)) throw std::runtime_error(error);
            if (allowReveal && reveal) if (const auto col=builder.revealedColumn();col && (*col<firstColumn || *col>=firstColumn+r.columns))
                return page(r,epoch,selected,*col>r.columns/2 ? *col-r.columns/2 : 0,false);
            const auto begin=start+fragment.firstByte,end=begin+fragment.text.size();
            if (backend) fragment.tokens=backend->syntax(identity,r.revision,selected,begin,fragment.text.size());
            else {
            auto token=std::lower_bound(tokens.begin(),tokens.end(),begin,[](const auto &t,auto b) { return t.start+t.length<=b; });
            for (;token!=tokens.end() && token->start<end;++token) {
                const auto a=std::max<std::uint64_t>(begin,token->start),b=std::min<std::uint64_t>(end,token->start+token->length);
                if (a<b) fragment.tokens.push_back({static_cast<std::size_t>(a-begin),static_cast<std::size_t>(b-a),token->role});
            }
            }
            if (markedRevision==r.revision) for (const auto &match:marks) {
                const auto a=std::max<std::uint64_t>(begin,match.offset),b=std::min<std::uint64_t>(end,match.offset+match.length);
                if (a>=b) continue;
                const auto left=static_cast<std::size_t>(a-begin),right=static_cast<std::size_t>(b-begin);
                std::vector<cyui::CodeToken> next;
                for (const auto &t:fragment.tokens) {
                    if (t.start+t.length<=left || t.start>=right) next.push_back(t);
                    else {
                        if (t.start<left) next.push_back({t.start,left-t.start,t.role});
                        if (t.start+t.length>right) next.push_back({right,t.start+t.length-right,t.role});
                    }
                }
                next.push_back({left,right-left,cyui::TokenRole::warning});
                std::sort(next.begin(),next.end(),[](const auto &a,const auto &b) { return a.start<b.start; });
                fragment.tokens=std::move(next);
            }
            bytes+=fragment.text.size()+fragment.tokens.size()*sizeof(cyui::CodeToken)+sizeof(cyui::PagedCodeLine);tokenCount+=fragment.tokens.size();
            if (bytes>r.maxBytes || tokenCount>65536) throw std::runtime_error("Page preparation exceeds its byte/token budget");
            p.lines.push_back(std::move(fragment));
        }
        return p;
    }
    void read(cyui::PagedCodeRead r) {
        const auto epoch=++readEpoch;const auto selected=language;activeRead=r.id;
        enqueue([this,r,epoch,selected] {
            Completion c;c.kind=Completion::page;c.request=r.id;c.expected=r.revision;
            try { if (epoch!=readEpoch) return;c.value=page(r,epoch,selected,r.firstColumn);c.status=nativeStatus; }
            catch (const std::exception &e) { if (epoch!=readEpoch) return;c.error=e.what(); }
            post(std::move(c));
        },1);
    }
    void mutate(cyui::PagedCodeReplacement proposal) {
        ui.busy=true;activeEdit=proposal.id;++readEpoch;++searchEpoch;
        enqueue([this,p=std::move(proposal)] {
            Completion c;c.kind=Completion::edited;c.request=p.id;c.expected=p.revision;
            try {
                guard(p.revision,true);if (p.document!=identity) throw std::runtime_error("Wrong paged document identity");
                if (backend) {
                    const auto prepared=backend->prepare(p);
                    struct Cleanup { PagedTextBackend &backend;const PagedTextPreparedEdit &edit;~Cleanup() { backend.discard(edit); } } cleanup{*backend,prepared};
                    if (prepared.token.empty() || prepared.document!=identity || prepared.revision!=p.revision)
                        throw std::runtime_error("Invalid native prepared edit identity");
                    const auto committed=backend->apply(prepared);
                    if (committed.status.revision==p.revision) throw std::runtime_error("Native apply did not issue a committed revision");
                    adopt(committed.status);c.status=nativeStatus;c.selection=committed.selection;post(std::move(c));return;
                }
                const auto begin=offset(p.selection.first()),end=offset(p.selection.last());
                const auto text=insertion(p.text,nativeStatus.newline);
                auto next=p.selection.first();Cursor cursor{next};cursor.advance(text);next=cursor.position;
                const auto before=native->snapshot(),after=native->edited(begin,end-begin,text);
                // Reserve all potentially allocating transaction state before publication.
                if (before!=after) history.reserve(historyIndex+1);
                native->commit(after);
                try { refresh(); } catch (...) { native->commit(before);throw; }
                if (before!=after) {
                    history.resize(historyIndex);history.push_back({before,after,p.selection,{next,next}});++historyIndex;
                }
                ++nativeStatus.revision;nativeStatus.canUndo=historyIndex>0;nativeStatus.canRedo=historyIndex<history.size();
                c.status=nativeStatus;c.selection={next,next};
            } catch (const std::exception &e) { c.error=e.what(); }
            post(std::move(c));
        });
    }
    void invalidatePresentation() {
        ++readEpoch;
        auto options=view.options();--options.maxPageBytes;view.setOptions(options);
        ++options.maxPageBytes;view.setOptions(options);
    }
    void applyStatus(const Status &value) {
        auto matches=std::move(ui.matches);const auto generation=ui.searchGeneration,searched=ui.searchedRevision;
        const bool searching=ui.searching,truncated=ui.truncated;
        ui=value;ui.matches=std::move(matches);ui.searchGeneration=generation;ui.searchedRevision=searched;
        ui.searching=searching;ui.truncated=truncated;
        auto options=view.options();options.newline=ui.newline;view.setOptions(options);
    }
    bool available() {
        if (!cancelled.empty() || !rejected.empty()) { ui.message="Recover cancelled input before editing or saving.";return false; }
        if (!ui.ready || ui.busy || activeEdit || actionPending || queuedInput || waitingForView) { ui.message="Wait for the current paged operation.";return false; }
        return true;
    }
};
PagedTextEditor::PagedTextEditor(cyui::ItemId id,const std::filesystem::path &path,const std::filesystem::path &work)
    :impl_(std::make_unique<Impl>(id,path,work)) {}
PagedTextEditor::PagedTextEditor(cyui::ItemId id,std::shared_ptr<PagedTextBackend> backend)
    :impl_(std::make_unique<Impl>(id,std::move(backend))) {}
void PagedTextEditor::refresh() {
    auto &s=*impl_;if (!s.backend) return;s.invalidatePresentation();++s.searchEpoch;s.searchKey.clear();s.ui.searching=false;
    s.enqueue([&s] {
        Impl::Completion c;c.kind=Impl::Completion::presentation;
        try { s.adopt(s.backend->state(s.identity));c.status=s.nativeStatus; }
        catch (const std::exception &e) { c.error=e.what(); }s.post(std::move(c));
    },3);
}
bool PagedTextEditor::checkpoint() {
    auto &s=*impl_;if (!s.available() || unsettled()) return false;
    if (!s.backend) { s.ui.message="Standalone durable paged recovery is unavailable.";return false; }
    s.ui.busy=true;const auto revision=s.ui.revision;
    s.enqueue([&s,revision] {
        Impl::Completion c;c.kind=Impl::Completion::presentation;
        try { s.guard(revision);s.adopt(s.backend->checkpoint(s.identity,revision));c.status=s.nativeStatus; }
        catch (const std::exception &e) { c.error=e.what(); }s.post(std::move(c));
    });return true;
}
PagedTextEditor::~PagedTextEditor()=default;
const PagedTextEditor::Status &PagedTextEditor::status() const { return impl_->ui; }
cyui::PagedCodeEditor &PagedTextEditor::view() { return impl_->view; }
void PagedTextEditor::poll() {
    auto &s=*impl_;std::deque<Impl::Completion> values;
    { std::lock_guard lock(s.mutex);values.swap(s.completed); }
    for (auto &c:values) {
        using C=Impl::Completion;
        if (c.kind==C::page) {
            const auto pending=s.view.pendingRead();
            if (!pending || pending->id!=c.request || pending->revision!=c.expected) continue;
            if (c.error.empty()) { s.view.supplyPage(c.value);s.ui.syntaxMessage=c.status.syntaxMessage; }
            else s.view.failRead(c.request,c.error);
        } else if (c.kind==C::searched) {
            if (c.search!=s.searchEpoch || c.expected!=s.ui.revision) continue;
            s.ui.searching=false;++s.ui.searchGeneration;s.ui.searchedRevision=c.expected;
            s.ui.matches=std::move(c.status.matches);s.ui.truncated=c.status.truncated;s.invalidatePresentation();
            if (!c.error.empty()) s.ui.message=c.error;else if (!c.status.message.empty()) s.ui.message=c.status.message;
            if (s.navigateAfterSearch) { const auto previous=*s.navigateAfterSearch;s.navigateAfterSearch.reset();find(previous); }
        } else if (c.kind==C::copied) {
            s.actionPending=false;
            if (c.expected!=s.ui.revision) { s.ui.message="Clipboard result discarded after a revision change.";continue; }
            if (c.error.empty()) {
                try {
                    ImGui::SetClipboardText(c.text.c_str());
                    const auto *copied=ImGui::GetClipboardText();
                    std::size_t copiedSize=0;if (copied) while (copiedSize<=clipboardBudget && copied[copiedSize]) ++copiedSize;
                    if (!copied || copiedSize!=c.text.size() || std::string_view(copied,copiedSize)!=c.text)
                        throw std::runtime_error("Clipboard publication failed; selection was not cut.");
                    if (c.cut && s.view.selection().anchor==c.selection.anchor && s.view.selection().caret==c.selection.caret)
                        s.queuedInput=s.view.replaceSelection("");
                } catch (const std::exception &e) { s.ui.message=e.what(); }
            } else s.ui.message=c.error;
        } else if (c.kind==C::edited) {
            s.activeEdit=0;s.ui.busy=false;s.searchKey.clear();s.ui.searching=false;
            if (c.error.empty()) {
                s.applyStatus(c.status);
                if (!s.view.acceptEdit(c.request,s.document(),c.selection)) {
                    // Native commit is authoritative even if a host cancelled the proposal.
                    s.view.setDocument(s.document());s.ui.message="Committed edit; view acknowledgement was cancelled.";
                }
                s.searchKey.clear();s.ui.matches.clear();++s.ui.searchGeneration;s.ui.searching=false;
            } else { s.view.rejectEdit(c.request,c.error);s.ui.message=c.error; }
        } else {
            s.ui.busy=false;
            if (!c.error.empty()) { s.ui.message=c.error;s.searchKey.clear();s.ui.searching=false; }
            else {
                const auto selection=c.kind==C::changed ? c.selection : s.view.selection();
                s.applyStatus(c.status);s.ui.busy=s.activeEdit!=0;s.view.setDocument(s.document());
                if (c.kind==C::changed || c.kind==C::saved) s.view.setSelection(selection);
                if (c.kind==C::changed || c.kind==C::saved) {
                    s.searchKey.clear();s.ui.matches.clear();s.ui.searching=false;++s.ui.searchGeneration;
                }
            }
        }
    }
}
std::vector<Command> PagedTextEditor::draw(const char *id,ImVec2 size) {
    poll();auto &s=*impl_;std::vector<Command> commands;
    if (!s.ui.ready) { ImGui::TextWrapped("%s",cyui::tr(s.ui.message.c_str()).c_str());return commands; }
    if (!s.ui.message.empty()) ImGui::TextWrapped("%s",cyui::tr(s.ui.message.c_str()).c_str());
    if (!s.ui.syntaxMessage.empty()) ImGui::TextWrapped("%s",cyui::tr(s.ui.syntaxMessage.c_str()).c_str());
    if (!s.cancelled.empty() || !s.rejected.empty()) return commands;
    if (s.ui.busy) {
        ImGui::TextUnformatted(cyui::tr("Native operation / index in progress...").c_str());
        // While a proposal awaits native acknowledgement, CyUI must still see
        // frames and retain bounded typing/caret input behind that proposal.
        // Hiding the widget here would drop character events at EndFrame.
        if (!s.activeEdit) return commands;
    }
    auto result=s.view.draw(id,size);
    s.waitingForView=result.waitingForEdit;
    if (result.cancelledRead) ++s.readEpoch;
    for (auto &input:result.cancelledInput) s.cancelled.push_back(std::move(input));
    if (result.edit.phase==cyui::EditPhase::cancel) {
        const auto &p=result.edit.value;
        const auto duplicate=std::any_of(s.rejected.begin(),s.rejected.end(),[&](const auto &v) { return v.id==p.id && v.document==p.document && v.revision==p.revision; });
        if (!duplicate) s.rejected.push_back(std::move(result.edit.value));
        s.queuedInput=false;
    }
    if (result.edit.phase==cyui::EditPhase::commit) { s.queuedInput=false;s.mutate(std::move(result.edit.value)); }
    if (result.read) s.read(*result.read);
    for (const auto &action:result.actions) {
        if (action.document!=s.identity || action.revision!=s.ui.revision) continue;
        if (action.command==Command::find || action.command==Command::save || action.command==Command::findNext || action.command==Command::findPrevious)
            commands.push_back(action.command);
        else command(action.command);
    }
    return commands;
}
void PagedTextEditor::setLanguage(cyui::CodeLanguage value) {
    auto &s=*impl_;if (s.language==value) return;s.language=value;s.invalidatePresentation();
}
void PagedTextEditor::setReadOnly(bool value) {
    auto &s=*impl_;if (!s.available() || s.ui.readOnly==value) return;
    s.ui.busy=true;s.invalidatePresentation();
    s.enqueue([&s,value] { Impl::Completion c;c.kind=Impl::Completion::presentation;
        try {
            if (s.backend) s.adopt(s.backend->setReadOnly(s.identity,s.nativeStatus.revision,value));
            else s.nativeStatus.readOnly=value;
            c.status=s.nativeStatus;
        } catch (const std::exception &e) { c.error=e.what(); }s.post(std::move(c)); });
}
bool PagedTextEditor::command(Command command) {
    auto &s=*impl_;if (!s.available()) return false;
    const auto revision=s.ui.revision;const auto selection=s.view.selection();
    if (command==Command::save) return save();
    if (command==Command::copy || command==Command::cut) {
        if (selection.empty() || (command==Command::cut && s.ui.readOnly)) return false;
        s.actionPending=true;
        s.enqueue([&s,revision,selection,command] {
            Impl::Completion c;c.kind=Impl::Completion::copied;c.expected=revision;c.selection=selection;c.cut=command==Command::cut;
            try {
                s.guard(revision,c.cut);
                if (s.backend) {
                    c.text=s.backend->copy(s.identity,revision,selection,clipboardBudget);validateEditorText(c.text);
                    s.post(std::move(c));return;
                }
                const auto begin=s.offset(selection.first()),end=s.offset(selection.last());
                if (end-begin>clipboardBudget) throw std::runtime_error("Clipboard selection exceeds the 4 MiB export budget.");
                for (auto at=begin;at<end;) { const auto text=s.native->read(at,static_cast<std::size_t>(std::max<std::uint64_t>(4,std::min<std::uint64_t>(chunkBytes,end-at))));
                    const auto take=static_cast<std::size_t>(std::min<std::uint64_t>(text.size(),end-at));c.text.append(text,0,take);at+=take; }
            } catch (const std::exception &e) { c.error=e.what(); }
            s.post(std::move(c));
        });return true;
    }
    if (command!=Command::undo && command!=Command::redo) return false;
    if (s.ui.readOnly) return false;
    s.ui.busy=true;++s.readEpoch;++s.searchEpoch;
    s.enqueue([&s,revision,command] {
        Impl::Completion c;c.kind=Impl::Completion::changed;
        try {
            s.guard(revision,true);
            if (s.backend) {
                const auto committed=s.backend->undo(s.identity,revision,command==Command::redo);
                if (committed.status.revision==revision) throw std::runtime_error("Native Undo did not issue a committed revision");
                s.adopt(committed.status);c.status=s.nativeStatus;c.selection=committed.selection;s.post(std::move(c));return;
            }
            const bool undo=command==Command::undo;
            if ((undo && !s.historyIndex) || (!undo && s.historyIndex==s.history.size())) throw std::runtime_error("No native transaction available.");
            const auto &entry=s.history[undo ? s.historyIndex-1 : s.historyIndex];
            const auto previous=s.native->snapshot();s.native->commit(undo ? entry.before : entry.after);
            try { s.refresh(); } catch (...) { s.native->commit(previous);throw; }
            if (undo) --s.historyIndex;else ++s.historyIndex;
            ++s.nativeStatus.revision;s.refresh();c.status=s.nativeStatus;c.selection=undo ? entry.previous : entry.next;
        } catch (const std::exception &e) { c.error=e.what(); }
        s.post(std::move(c));
    });return true;
}
bool PagedTextEditor::paste() {
    auto &s=*impl_;if (!s.available() || s.ui.readOnly) return false;
    const auto *value=ImGui::GetClipboardText();if (!value) return false;
    std::size_t length=0;while (length<=1024*1024 && value[length]) ++length;
    if (length>1024*1024) { s.ui.message="Insertion exceeds the 1 MiB operation budget";return false; }
    s.queuedInput=s.view.replaceSelection(std::string_view(value,length));return s.queuedInput;
}
bool PagedTextEditor::save(const std::filesystem::path &destination) {
    auto &s=*impl_;if (!s.available() || unsettled() || s.ui.readOnly) return false;
    s.ui.busy=true;s.ui.message="Saving the complete native document...";++s.readEpoch;++s.searchEpoch;
    const auto revision=s.ui.revision;
    s.enqueue([&s,revision,destination] {
        Impl::Completion c;c.kind=Impl::Completion::saved;
        try {
            s.guard(revision,true);
            if (s.backend) {
                s.adopt(s.backend->save(s.identity,revision,destination));c.status=s.nativeStatus;s.post(std::move(c));return;
            }
            if (destination.empty()) s.native->save();
            else { s.native->saveAs(destination);s.nativeStatus.path=destination; }
            s.refresh();c.status=s.nativeStatus;
        } catch (const std::exception &e) { c.error=e.what(); }
        s.post(std::move(c));
    });return true;
}
bool PagedTextEditor::search(const std::string &query,bool sensitive,bool words) {
    auto &s=*impl_;if (!s.ui.ready || s.ui.busy) return false;
    const auto key=std::to_string(s.ui.revision)+"/"+std::to_string(sensitive)+std::to_string(words)+"/"+query;
    if (s.searchKey==key) return !s.ui.searching;
    s.searchKey=key;s.ui.matches.clear();s.ui.searching=false;++s.ui.searchGeneration;
    const auto epoch=++s.searchEpoch,revision=s.ui.revision;
    if (query.empty()) {
        s.enqueue([&s,revision] { s.marks.clear();s.markedRevision=revision; },2);
        s.invalidatePresentation();return true;
    }
    if (query.size()>4096) { s.ui.message="Search query must contain 1 to 4096 bytes";return false; }
    s.ui.searching=true;
    s.enqueue([&s,query,sensitive,words,epoch,revision] {
        Impl::Completion c;c.kind=Impl::Completion::searched;c.expected=revision;c.search=epoch;
        try {
            s.guard(revision);validateEditorText(query);
            if (s.backend) {
                auto found=s.backend->search(s.identity,revision,query,sensitive,words,2000,[&s,epoch] { return epoch!=s.searchEpoch; });
                if (found.matches.size()>2000) throw std::runtime_error("Native search exceeds its result budget");
                c.status.matches=std::move(found.matches);c.status.truncated=found.truncated;
            } else {
            std::uint64_t at=0,base=0,processed=0,lastMatchEnd=0;std::string window;Cursor cursor;
            const auto head=s.native->read(0,4);if (head.starts_with("\xef\xbb\xbf")) at=base=processed=3;
            while (at<s.native->bytes() || !window.empty()) {
                if (epoch!=s.searchEpoch) return;
                if (at<s.native->bytes()) { const auto part=s.native->read(at,chunkBytes);window+=part;at+=part.size(); }
                const bool final=at==s.native->bytes();
                std::size_t safe=final ? window.size() : window.size()>query.size()+8 ? window.size()-query.size()-8 : 0;
                while (safe && safe<window.size() && (static_cast<unsigned char>(window[safe])&0xc0)==0x80) --safe;
                if (!safe) { if (final) break;continue; }
                const auto found=text::findTextMatches(window,query,sensitive,words,2001,static_cast<std::size_t>(std::max(processed,lastMatchEnd)-base));
                for (const auto &match:found.matches) {
                    if (base+match.offset<processed || match.offset>=safe) continue;
                    // UTF-16 line positions cannot identify the middle of CRLF.
                    // Keep physical-byte search semantics, never select extra bytes.
                    const auto finish=match.offset+match.length;
                    if ((window[match.offset]=='\n' && match.offset && window[match.offset-1]=='\r') ||
                        (window[finish-1]=='\r' && finish<window.size() && window[finish]=='\n')) {
                        c.status.message="Search must include the complete CRLF pair.";continue;
                    }
                    Cursor begin=cursor;begin.advance(std::string_view(window).substr(0,match.offset));
                    Cursor end=begin;end.advance(std::string_view(window).substr(match.offset,match.length));
                    if (c.status.matches.size()==2000) { c.status.truncated=true;break; }
                    auto preview=window.substr(match.offset,std::min<std::size_t>(512,window.size()-match.offset));
                    if (const auto eol=preview.find_first_of("\r\n");eol!=std::string::npos) preview.resize(eol);
                    if (!preview.empty()) {
                        auto last=preview.size()-1;while (last && (static_cast<unsigned char>(preview[last])&0xc0)==0x80) --last;
                        const auto lead=static_cast<unsigned char>(preview[last]);
                        const std::size_t width=lead<0x80 ? 1 : lead<0xe0 ? 2 : lead<0xf0 ? 3 : 4;
                        if (preview.size()-last<width) preview.resize(last);
                    }
                    c.status.matches.push_back({base+match.offset,match.length,{begin.position,end.position},std::move(preview)});lastMatchEnd=base+match.offset+match.length;
                }
                if (c.status.truncated || final) break;
                processed=base+safe;
                // Keep at least one complete scalar of left context for whole-word matching.
                auto drop=safe>4 ? safe-4 : 0;
                while (drop && (static_cast<unsigned char>(window[drop])&0xc0)==0x80) --drop;
                cursor.advance(std::string_view(window).substr(0,drop));window.erase(0,drop);base+=drop;
            }
            }
        } catch (const std::exception &e) { c.error=e.what(); }
        if (epoch!=s.searchEpoch) return;
        for (const auto &m:c.status.matches) {
            if (!m.length || m.offset>s.nativeStatus.bytes || m.length>s.nativeStatus.bytes-m.offset ||
                !(m.range.first()<m.range.last()) || m.range.last().line>=s.nativeStatus.lines || m.preview.size()>512) {
                c.status.matches.clear();c.error="Invalid native paged search range";break;
            }
        }
        s.marks=c.status.matches;s.markedRevision=revision;
        s.post(std::move(c));
    },2);return false;
}
bool PagedTextEditor::find(bool previous,bool wrap) {
    auto &s=*impl_;if (s.ui.searching) { s.navigateAfterSearch=previous;return false; }
    if (s.ui.searchedRevision!=s.ui.revision || s.ui.matches.empty()) return false;
    const auto selection=s.view.selection();const auto point=previous ? selection.first() : selection.last();
    const Match *hit=nullptr;
    if (previous) { for (const auto &m:s.ui.matches) if (m.range.first()<point) hit=&m; }
    else for (const auto &m:s.ui.matches) if (!(m.range.first()<point)) { hit=&m;break; }
    if (!hit && wrap) hit=previous ? &s.ui.matches.back() : &s.ui.matches.front();
    return hit && s.view.setSelection(hit->range);
}
bool PagedTextEditor::replace(const std::string &value) { auto &s=*impl_;if (!s.available() || s.ui.readOnly) return false;s.queuedInput=s.view.replaceSelection(value);return s.queuedInput; }
void PagedTextEditor::selectAll() {
    auto &s=*impl_;if (!s.available()) return;s.ui.busy=true;
    const auto revision=s.ui.revision;
    s.enqueue([&s,revision] {
        Impl::Completion c;c.kind=Impl::Completion::changed;
        try {
            s.guard(revision);const auto last=s.nativeStatus.lines-1;
            auto at=s.offset({last,0});Cursor cursor{{last,0}};
            while (at<s.nativeStatus.bytes) { const auto part=s.readBytes(at,chunkBytes);cursor.advance(part);at+=part.size(); }
            c.status=s.nativeStatus;c.selection={{0,0},cursor.position};
        } catch (const std::exception &e) { c.error=e.what(); }
        s.post(std::move(c));
    });
}
void PagedTextEditor::goTo(std::uint64_t line) {
    auto &s=*impl_;if (!s.available()) return;Position p{std::min(line,s.ui.lines-1),0};s.view.setSelection({p,p});
}
bool PagedTextEditor::unsettled() const { const auto &s=*impl_;return s.activeEdit || s.actionPending || s.queuedInput || s.waitingForView || !s.cancelled.empty() || !s.rejected.empty(); }
const std::vector<cyui::PagedCodeInput> &PagedTextEditor::cancelledInput() const { return impl_->cancelled; }
const std::vector<cyui::PagedCodeReplacement> &PagedTextEditor::cancelledEdits() const { return impl_->rejected; }
std::string PagedTextEditor::cancelledText() const {
    std::string value;
    for (const auto &p:impl_->rejected) {
        value+="[cancelled replacement: document "+std::to_string(p.document)+", revision "+std::to_string(p.revision)+
            ", UTF-16 "+std::to_string(p.selection.first().line)+":"+std::to_string(p.selection.first().character)+".."+
            std::to_string(p.selection.last().line)+":"+std::to_string(p.selection.last().character)+"]\n"+p.text+"\n";
    }
    for (const auto &input:impl_->cancelled) {
        if (input.kind==cyui::PagedCodeInputKind::text) value+=input.text;
        else value+="\n[pending command "+std::to_string(static_cast<int>(input.kind))+"]\n";
    }return value;
}
void PagedTextEditor::acknowledgeCancelledInput() { impl_->cancelled.clear();impl_->rejected.clear();impl_->ui.message.clear();impl_->view.retryRead(); }
}
