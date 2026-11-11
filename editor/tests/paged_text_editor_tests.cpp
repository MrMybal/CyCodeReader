// Prepared acceptance fixture. Compile with /Zs while native execution is suspended.
// Opt-in execution later: cycode_paged_editor_tests <new synthetic fixture directory>.
#include "cycode/PagedTextEditor.h"
#include <cyui/Core.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace cycode;
static void require(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
static void frame(PagedTextEditor &editor) {
    ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({900,600});
    ImGui::Begin("Paged fixture");editor.draw("##paged");ImGui::End();ImGui::Render();
}
template<class Predicate> static void wait(PagedTextEditor &editor,Predicate done) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    do {
        frame(editor);if (done()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now()<deadline);
    throw std::runtime_error("Native paged operation timed out");
}
static void ready(PagedTextEditor &editor) { wait(editor,[&] { return editor.status().ready && !editor.status().busy && !editor.unsettled(); }); }
static std::string bytes(const std::filesystem::path &p) { std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}}; }
static void write(const std::filesystem::path &p,const std::string &s) { std::ofstream out(p,std::ios::binary);out<<s;require(bool(out),"Fixture write failed"); }
static void small(const std::filesystem::path &root) {
    const auto source=root/"unicode.cpp",saved=root/"saved.cpp";
    const std::string original="\xef\xbb\xbf/* context\r\n\t\xf0\x9f\x9b\xa0 name */\r\nlast\rfinal\n";
    write(source,original);PagedTextEditor editor(101,source,root);editor.setLanguage(cyui::CodeLanguage::cpp);ready(editor);
    require(editor.status().lines==5 && editor.status().newline=="\r\n","BOM/mixed newline index");
    editor.search("\r\n",true,false);wait(editor,[&] { return !editor.status().searching; });
    require(editor.status().matches.size()==2,"Physical CRLF search lost complete newline ranges");
    editor.search("\n",true,false);wait(editor,[&] { return !editor.status().searching; });
    require(editor.status().matches.size()==1,"Search selected an unaddressable half of CRLF");
    // UTF-16 character 1 is after a tab; the supplementary scalar occupies 1..3.
    require(editor.view().setSelection({{1,1},{1,3}}),"Unicode selection");
    require(editor.replace("changed\nvalue"),"Edit proposal");
    wait(editor,[&] { return editor.status().revision==2 && !editor.unsettled(); });
    require(editor.status().dirty && editor.status().canUndo,"Commit did not create native Undo");
    require(editor.command(cyui::PagedCodeCommand::undo),"Undo action");
    wait(editor,[&] { return editor.status().revision==3 && !editor.status().busy; });
    require(!editor.status().dirty && editor.status().canRedo,"Undo did not restore native baseline");
    require(editor.command(cyui::PagedCodeCommand::redo),"Redo action");
    wait(editor,[&] { return editor.status().revision==4 && !editor.status().busy; });
    require(editor.view().setSelection({{0,0},{0,0}}),"Rejected proposal anchor");
    require(editor.replace(std::string(600000,'\n')),"Oversized normalized insertion proposal");
    wait(editor,[&] { return !editor.cancelledEdits().empty(); });
    require(editor.status().revision==4 && editor.cancelledEdits().front().text.size()==600000,"Rejected input or revision was lost");
    require(!editor.save(saved),"Unacknowledged cancelled input allowed publication");
    editor.acknowledgeCancelledInput();frame(editor);
    require(editor.save(saved),"Native Save As request");
    wait(editor,[&] { return !editor.status().busy; });
    auto expected=original;expected.replace(expected.find("\xf0\x9f\x9b\xa0"),4,"changed\r\nvalue");
    require(bytes(saved)==expected && bytes(source)==original,"Full save/BOM/CRLF/original preservation");
    require(editor.save(source),"Existing destination request");
    wait(editor,[&] { return !editor.status().busy; });
    require(!editor.status().message.empty() && bytes(source)==original,"Existing target overwritten");
    write(saved,"external");require(editor.save(),"Conflict save request");
    wait(editor,[&] { return !editor.status().busy; });
    require(bytes(saved)=="external" && !editor.status().message.empty(),"Disk conflict bypassed");
    editor.setReadOnly(true);wait(editor,[&] { return !editor.status().busy; });
    require(!editor.replace("blocked") && !editor.command(cyui::PagedCodeCommand::undo),"Read-only bypassed");
}
static void longFirstLine(const std::filesystem::path &root) {
    const auto source=root/"long-first-line.txt";
    write(source,std::string(3*65536-1,'x')+"\r\nnext\r\n");
    PagedTextEditor editor(104,source,root);ready(editor);
    require(editor.status().newline=="\r\n","First CRLF beyond a page boundary became LF");
}
class BlockedBackend final : public PagedTextBackend {
  public:
    std::atomic<bool> entered=false,release=false;
    PagedTextStatus state(cyui::ItemId) override {PagedTextStatus s;s.ready=true;s.busy=false;s.revision=1;s.bytes=3;s.lines=1;return s;}
    uint64_t offset(cyui::ItemId,uint64_t,cyui::PagedCodePosition p) override{return p.character;}
    PagedTextBytes readBytes(cyui::ItemId,uint64_t,uint64_t at,size_t count) override {
        const auto text=std::string("abc").substr(size_t(at),count);return {text,at+text.size()};
    }
    std::vector<cyui::CodeToken> syntax(cyui::ItemId,uint64_t,cyui::CodeLanguage,uint64_t,size_t) override{return {};}
    PagedTextPreparedEdit prepare(const cyui::PagedCodeReplacement &p) override{return {"blocked",p.document,p.revision};}
    PagedTextCommit apply(const PagedTextPreparedEdit &) override {
        entered=true;while(!release)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        throw std::runtime_error("Synthetic preflight rejection");
    }
    void discard(const PagedTextPreparedEdit &) noexcept override{}
    PagedTextCommit undo(cyui::ItemId,uint64_t,bool) override{throw std::runtime_error("Unused fixture method");}
    PagedTextStatus setReadOnly(cyui::ItemId id,uint64_t,bool) override{return state(id);}
    std::string copy(cyui::ItemId,uint64_t,cyui::PagedCodeSelection,size_t) override{return {};}
    PagedTextSearch search(cyui::ItemId,uint64_t,const std::string &,bool,bool,size_t,const Cancelled &) override{return {};}
    PagedTextStatus save(cyui::ItemId id,uint64_t,const std::filesystem::path &) override{return state(id);}
    PagedTextStatus checkpoint(cyui::ItemId id,uint64_t) override{return state(id);}
};
static void pendingTyping() {
    auto backend=std::make_shared<BlockedBackend>();
    PagedTextEditor editor(105,backend);
    // Release the worker even if a regression assertion throws.
    struct Release {std::shared_ptr<BlockedBackend> value;~Release(){value->release=true;}} guard{backend};
    ready(editor);wait(editor,[&]{return !editor.view().pendingRead();});
    auto &io=ImGui::GetIO();io.AddMousePosEvent(100,50);io.AddMouseButtonEvent(0,true);frame(editor);
    io.AddMouseButtonEvent(0,false);frame(editor);
    editor.view().setSelection({{0,0},{0,0}});require(editor.replace("first"),"Pending edit was not proposed");
    wait(editor,[&]{return backend->entered.load();});
    io.AddInputCharactersUTF8("queued");frame(editor);
    backend->release=true;wait(editor,[&]{return !editor.cancelledEdits().empty();});
    require(editor.cancelledText().find("queued")!=std::string::npos,"Typing behind a native proposal was silently lost");
    require(editor.cancelledText().find("first")!=std::string::npos,"Rejected proposal was silently lost");
}
static void large(const std::filesystem::path &root) {
    const auto source=root/"96MiB.txt",saved=root/"96MiB-saved.txt";
    const std::string block(65535,'x');
    { std::ofstream out(source,std::ios::binary);for (int i=0;i<1536;++i) out<<block<<'\n'; }
    PagedTextEditor editor(102,source,root);ready(editor);
    require(editor.status().bytes==96ull*1024*1024,"Large import extent");
    editor.goTo(1400);frame(editor);editor.goTo(2);frame(editor); // supersede in-flight page
    editor.search("xxx",true,true);wait(editor,[&] { return !editor.status().searching; });
    require(editor.status().matches.empty(),"Whole-word search crossed a chunk boundary");
    editor.view().setSelection({{100,65534},{103,1}});require(editor.replace("multi-page"),"Cross-page replacement");
    wait(editor,[&] { return editor.status().revision==2 && !editor.unsettled(); });
    require(editor.save(saved),"Complete large Save As");wait(editor,[&] { return !editor.status().busy; });
    require(std::filesystem::file_size(source)==96ull*1024*1024,"Save As changed original");
    require(std::filesystem::file_size(saved)==96ull*1024*1024-(3ull*65536-65533)+10,"Visible page substituted for document");
    // Verify every output byte without reading the 96 MiB file into the widget.
    std::ifstream in(saved,std::ios::binary);std::uint64_t count=0;char c;
    const std::uint64_t begin=100ull*65536+65534;
    while (in.get(c)) { char expected;
        if (count<begin) expected=(count%65536==65535) ? '\n' : 'x';
        else if (count<begin+10) expected=std::string_view("multi-page")[static_cast<std::size_t>(count-begin)];
        else { const auto originalOffset=count-10+(3ull*65536-65533);expected=originalOffset%65536==65535 ? '\n' : 'x'; }
        require(c==expected,"Large streamed save differs from expected bytes");++count;
    }
    const auto line=root/"long-line.txt";{ std::ofstream out(line,std::ios::binary);for (int i=0;i<1536;++i) out<<block<<'x'; }
    PagedTextEditor horizontal(103,line,root);ready(horizontal);horizontal.view().scrollTo(0,80*1024*1024);frame(horizontal);
    const auto obsolete=horizontal.view().pendingRead();horizontal.view().scrollTo(0,1);frame(horizontal);
    if (obsolete) require(!horizontal.view().supplyPage({obsolete->id,103,obsolete->revision,obsolete->firstColumn,{}}),"Obsolete page accepted");
    wait(horizontal,[&] { return !horizontal.view().pendingRead(); });
}
int main(int argc,char **argv) {
    try {
        require(argc==2,"Pass a new synthetic fixture directory");const auto root=std::filesystem::absolute(argv[1]);
        require(!std::filesystem::exists(root),"Fixture directory already exists");std::filesystem::create_directories(root);
        ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1000,700};io.DeltaTime=1.f/60;
        cyui::loadFonts();unsigned char *pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        small(root);longFirstLine(root);pendingTyping();large(root);ImGui::DestroyContext();std::cout<<"Paged adapter fixtures passed; synthetic files retained.\n";return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
