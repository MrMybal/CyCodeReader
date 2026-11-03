#include "Reader.h"
#include "SoftwareCapture.h"
#include "NativeProcess.h"
#include "cycode/Paths.h"
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace cycode::reader {
struct ReaderComparisonTests {
    static void require(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
    static void write(const std::filesystem::path &path,const std::string &bytes) { std::ofstream(path,std::ios::binary)<<bytes; }
    static void draw(Reader &reader) {
        ImGui::NewFrame();reader.comparisonPanel();ImGui::Render();
        require(ImGui::GetDrawData()->TotalVtxCount>0,"Comparison did not emit drawing data");
    }
    static void awaitComparison(Reader &reader) {
        require(reader.comparison_.pending.valid(),"No comparison worker started");reader.comparison_.pending.wait();draw(reader);
    }
    static void awaitContext(Reader &reader) {
        for (auto &provider : reader.fileProviders_) if (provider.pending.valid()) provider.pending.wait();
        reader.pollFileProviders();
    }
    static void historyFrame(Reader &reader) { ImGui::NewFrame();reader.historyPanel();ImGui::Render(); }
    static void awaitHistoryDiff(Reader &reader) {
        for (int n=0;n<5;++n) {
            if (reader.history_.diff.pending.valid()) reader.history_.diff.pending.wait();historyFrame(reader);
            if (!reader.history_.diff.pending.valid()) break;
        }
        require(reader.history_.diff.ready,reader.history_.diff.message.c_str());
    }
    static void git(const std::string &executable,const std::filesystem::path &root,const std::vector<std::string> &arguments) {
        std::vector<std::string> argv={executable,"-C",pathUtf8(root),"-c","user.name=Offline fixture","-c","user.email=fixture@example.invalid","-c","core.autocrlf=false"};argv.insert(argv.end(),arguments.begin(),arguments.end());
        const auto result=runProcess(argv,root,std::chrono::seconds(15),256*1024,nullptr,{{"GIT_CONFIG_NOSYSTEM","1"}}, {"GIT_DIR","GIT_WORK_TREE","GIT_INDEX_FILE","GIT_CONFIG_COUNT","GIT_CONFIG_PARAMETERS"});
        if (result.exitCode) throw std::runtime_error(result.output);
    }
    static int run(const std::filesystem::path &source,const std::filesystem::path &library,const std::string &executable) {
        const auto root=std::filesystem::temp_directory_path()/("reader-compare-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root/"plugins");std::filesystem::create_directories(root/"locales");
        std::filesystem::copy_file(library,root/"plugins"/library.filename());std::filesystem::copy_file(source/"plugins/git.json",root/"plugins/git.json");std::filesystem::copy_file(source/"locales/ui.json",root/"locales/ui.json");
        const auto left=root/utf8Path("gauche été.cpp"),right=root/"right.cpp";write(left,"int value=1;\n");write(right,"int value=2;\n");
        const auto repo=root/"repo";std::filesystem::create_directory(repo);git(executable,repo,{"init"});
        const auto tracked=repo/utf8Path("été [x].cpp");write(tracked,"int committed=1;\n");git(executable,repo,{"add","--",pathUtf8(tracked.filename())});git(executable,repo,{"commit","-m","Owned comparison fixture"});
        // A prepared invocation pins its native DLL even after its owner closes.
        std::function<Json()> pinned;
        { Plugins plugins;plugins.load(root/"plugins");require(plugins.describe().at("errors").empty(),"Git plugin could not load");pinned=plugins.prepare("cy.reader.git","context",{{"path",pathUtf8(tracked)}}); }
        require(pinned().value("available",false),"Prepared invocation did not retain its plugin library");pinned={};
        ImGui::CreateContext();
        try {
            auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1600,1100};io.DeltaTime=1.f/60;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
            {
                Reader reader({root,{},"graphite","en",true},nullptr);test::preloadCaptureGlyphs();unsigned char *pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);io.Fonts->SetTexID(ImTextureID(1));
                require(reader.fileProviders_.size()==1,"Git plugin did not contribute its interface provider");
                reader.open(left);const auto leftId=reader.active_;reader.execute("compare.select");reader.open(right);const auto rightId=reader.active_;
                reader.testAction({{"command","setText"},{"text","int value=3;\n"}});const auto before=reader.state().at("documents");
                reader.execute("compare.selected");require(reader.comparison_.before=="int value=1;\n" && reader.comparison_.after=="int value=3;\n","Open-tab comparison ignored unsaved text");
                require(!reader.comparison_.document.hunks().empty(),"Text change has no diff hunk");
                bool added=false,removed=false;for (const auto &row : reader.comparison_.document.rows()) { added|=row.kind==cyui::DiffKind::added;removed|=row.kind==cyui::DiffKind::removed; }
                require(added && removed,"Diff omitted additions or removals");
                for (const auto *theme : {"graphite","light-grey"}) for (const auto *locale : {"en","fr"}) for (float dpi : {1.f,2.f}) for (bool split : {false,true}) {
                    reader.execute("view.theme",{{"theme",theme}});reader.execute("view.locale",{{"locale",locale}});cyui::applyDpi(dpi);reader.comparison_.state.split=split;draw(reader);draw(reader);
                }
                require(reader.state().at("documents")==before,"Viewing comparison changed buffers, revisions, selection or Undo");
                reader.execute("compare.saved");require(reader.comparison_.before=="int value=2;\n","Saved version did not use the actual save baseline");
                write(right,"external disk edit\n");reader.execute("compare.saved");require(reader.comparison_.before=="int value=2;\n","Saved baseline silently became an external disk edit");
                reader.execute("compare.file",{{"path",pathUtf8(right)}});require(reader.comparison_.before=="external disk edit\n","File comparison did not read the selected disk snapshot");
                reader.execute("compare.file");require(reader.dialog_ && reader.dialog_->compare,"File picker was not routed to comparison");reader.dialog_->done=true;reader.pollDialog();require(reader.state().at("documents")==before,"Cancelled comparison picker changed documents");
                reader.execute("compare.selected");reader.testAction({{"command","setText"},{"text","int value=4;\n"}});require(reader.comparisonStale(),"Editing a source did not invalidate the diff snapshot");reader.refreshComparison();require(!reader.comparisonStale() && reader.comparison_.after=="int value=4;\n","Comparison refresh did not use current live text");
                reader.erase(leftId);require(reader.comparisonStale(),"Closing a source left an apparently current comparison");
                reader.open(tracked);reader.execute("compare.detect");awaitContext(reader);require(reader.providerReady(0,"head"),"Git working tree was not detected for the active file");
                const auto model=reader.providerMenuModel(0);cyui::TextFilter filter;filter.query="tampon head";
                require(cyui::filterMenuMatches(model.items,filter).size()==1,"French Git menu search did not find the HEAD action");
                reader.testAction({{"command","setText"},{"text","int live=2;\n"}});reader.active()->editor.SetReadOnlyEnabled(true);reader.active()->live->readOnly=true;
                const auto gitBefore=reader.state().at("documents");reader.execute("compare.provider",{{"provider",0},{"command","head"}});awaitComparison(reader);
                require(reader.comparison_.before=="int committed=1;\n" && reader.comparison_.after=="int live=2;\n","Git comparison did not include live unsaved text");
                require(reader.state().at("documents")==gitBefore,"Git comparison edited a read-only document");
                reader.active()->editor.SetReadOnlyEnabled(false);reader.active()->live->readOnly=false;
                const auto apiResult=reader.core_.dispatch("plugins.invoke",{{"id",reader.active_},{"expectedRevision",reader.active()->live->revision},{"plugin","cy.reader.git"},{"command","head"}});
                require(apiResult.at("result").at("comparison").at("after")=="int live=2;\n","Shared native plugin API did not use the same live document");
                reader.execute("compare.provider",{{"provider",0},{"command","head"}});reader.testAction({{"command","setText"},{"text","int changedDuringRequest=3;\n"}});awaitComparison(reader);
                require(!reader.comparison_.ready && !reader.comparison_.message.empty(),"Stale asynchronous Git result was accepted");reader.refreshComparison();awaitComparison(reader);require(reader.comparison_.ready,"Retry after a stale Git result failed");
                const auto committedBytes=readTextFileBytes(tracked);require(committedBytes=="int committed=1;\n","Git comparison saved the live document");
                reader.open(right);reader.pollFileProviders(true);awaitContext(reader);require(!reader.providerReady(0),"Git actions stayed enabled outside a repository");
                require(readTextFileBytes(left)=="int value=1;\n" && readTextFileBytes(right)=="external disk edit\n","Comparison changed source files on disk");
                reader.open(tracked);reader.execute("history.open",{{"provider",0}});require(reader.history_.pending.valid(),"History worker did not start");reader.history_.pending.wait();
                ImGui::NewFrame();reader.historyPanel();ImGui::Render();
                require(reader.history_.message.empty() && reader.history_.commits.size()==1,"File history did not load its initial revision");
                awaitHistoryDiff(reader);require(reader.history_.diff.before.empty() && reader.history_.diff.after=="int committed=1;\n","First selected commit did not automatically show its inline file diff");
                cyui::applyDpi(1.f);reader.execute("view.theme",{{"theme","graphite"}});if (auto *window=ImGui::FindWindowByName("Git history###reader-history")) ImGui::SetWindowSize(window,{1080,640});ImGui::NewFrame();reader.historyPanel();ImGui::Render();test::capture("git-history");
                reader.execute("history.compare",{{"separate",true}});awaitComparison(reader);require(reader.comparison_.ready && reader.comparison_.before=="int committed=1;\n","History revision did not compare with the live buffer");
                reader.execute("history.parent",{{"separate",true}});awaitComparison(reader);require(reader.comparison_.ready && reader.comparison_.before.empty(),"Initial revision did not compare with an empty parent");
                reader.testAction({{"command","setText"},{"text","int committed=1;\n"}});reader.active()->editor.SetCursor({0,0});reader.execute("history.selection",{{"provider",0}});reader.history_.pending.wait();
                ImGui::NewFrame();reader.historyPanel();ImGui::Render();require(reader.history_.message.empty() && reader.history_.commits.size()==1,"Current line history was not displayed");
                reader.execute("history.selection",{{"provider",0}});reader.testAction({{"command","setText"},{"text","changed while history loads\n"}});reader.history_.pending.wait();
                ImGui::NewFrame();reader.historyPanel();ImGui::Render();require(!reader.history_.message.empty(),"Stale asynchronous selection history was accepted");
                write(tracked,"int committed=9;\n");git(executable,repo,{"commit","-am","Second history revision"});
                reader.testAction({{"command","setText"},{"text","int unsaved=17;\n"}});const auto inlineBefore=reader.state().at("documents");reader.comparison_.open=false;
                reader.execute("history.open",{{"provider",0}});reader.history_.pending.wait();historyFrame(reader);
                require(reader.history_.commits.size()==2 && reader.history_.diff.pending.valid(),"Automatic inline revision worker did not start");
                // A faster second selection queues its request without waiting for
                // or displaying the previous selection's running result.
                reader.history_.selected=1;reader.execute("history.parent");awaitHistoryDiff(reader);
                require(reader.history_.diff.before.empty() && reader.history_.diff.after=="int committed=1;\n","Rapid selection accepted the previous commit's asynchronous diff");
                reader.history_.selected=0;reader.execute("history.parent");awaitHistoryDiff(reader);
                require(reader.history_.diff.before=="int committed=1;\n" && reader.history_.diff.after=="int committed=9;\n" && !reader.comparison_.open,"Inline history diff opened a separate window or used the wrong parent");
                for (const auto *theme : {"graphite","light-grey"}) for (const auto *locale : {"en","fr"}) for (float dpi : {1.f,2.f}) {
                    reader.execute("view.theme",{{"theme",theme}});reader.execute("view.locale",{{"locale",locale}});cyui::applyDpi(dpi);historyFrame(reader);historyFrame(reader);
                }
                require(reader.state().at("documents")==inlineBefore,"Inline diff viewing modified live text, selection or Undo");
                cyui::applyDpi(1.f);reader.execute("view.theme",{{"theme","graphite"}});reader.execute("view.locale",{{"locale","en"}});ImGui::SetWindowSize(ImGui::FindWindowByName("Git history###reader-history"),{1040,720});historyFrame(reader);historyFrame(reader);test::capture("git-history-inline");
                reader.execute("history.compare");historyFrame(reader);require(reader.history_.diff.pending.valid(),"Inline buffer comparison did not start");
                reader.testAction({{"command","setText"},{"text","int latest=23;\n"}});awaitHistoryDiff(reader);require(reader.history_.diff.before=="int committed=9;\n" && reader.history_.diff.after=="int latest=23;\n","Inline buffer comparison accepted an obsolete source revision");
                reader.history_.left=1;reader.history_.right=0;reader.execute("history.revisions");awaitHistoryDiff(reader);require(reader.history_.diff.before=="int committed=1;\n" && reader.history_.diff.after=="int committed=9;\n","Two selected revisions did not render inside history");
                const auto sourceId=reader.history_.document;reader.execute("history.parent");historyFrame(reader);reader.erase(sourceId);if (reader.history_.diff.pending.valid()) reader.history_.diff.pending.wait();historyFrame(reader);require(!reader.history_.diff.ready,"Closing the source accepted an orphaned inline diff");
            }
            ImGui::DestroyContext();std::filesystem::remove_all(root);
            std::cout<<"Comparisons: live tabs, saved/disk snapshots, split/unified EN/FR/DPI, source guards, real Git provider, read-only documents and stale async results passed without windows.\n";return 0;
        } catch (const std::exception &error) { std::cerr<<error.what()<<"\nFixture: "<<root<<'\n';ImGui::DestroyContext();return 1; }
    }
};
}
int main(int argc,char **argv) { return argc==4 ? cycode::reader::ReaderComparisonTests::run(cycode::utf8Path(argv[1]),cycode::utf8Path(argv[2]),argv[3]) : 1; }
