#include "Reader.h"
#include <cyui/Core.h>
#include <chrono>
#include <fstream>
#include <iostream>

namespace cycode::reader {
struct ReaderUpdateTests {
    static void require(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
    static void draw(Reader &reader) {
        ImGui::NewFrame();reader.updatePanel();ImGui::Render();
        require(ImGui::GetDrawData()->TotalVtxCount>0,"Updates panel has no drawing data");
    }
    static int run(const std::filesystem::path &source) {
        const auto root=std::filesystem::temp_directory_path()/("reader-update-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root/"locales");
        std::filesystem::copy_file(source/"locales/ui.json",root/"locales/ui.json");
        std::ofstream(root/"portable.flag")<<"portable";
        ImGui::CreateContext();
        try {
            auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1500,1100};io.DeltaTime=1.f/60;
            {
                Reader reader({root,{},"graphite","en",true},nullptr);
                unsigned char *pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(ImTextureID(1));
                reader.execute("new");reader.testAction({{"command","setText"},{"text","unsaved user document\n"}});
                const auto before=reader.state().at("documents");
                reader.execute("updates");
                require(reader.updateCheckOnStartup_,"New installations must check for updates at startup by default");
                for (const auto *theme : {"graphite","light-grey"}) for (const auto *locale : {"en","fr"}) for (float dpi : {1.f,2.f}) {
                    reader.execute("view.theme",{{"theme",theme}});reader.execute("view.locale",{{"locale",locale}});cyui::applyDpi(dpi);
                    draw(reader);draw(reader);
                }
                reader.checkUpdates();require(!reader.releaseCheck_.valid(),"Automated UI started a network check");
                reader.availableRelease_=ReaderRelease{"v0.2.0","Offline fixture notes",{"fixture.zip","",std::string(64,'a'),100}};
                reader.preparedUpdate_=PreparedUpdate{root/"fixture.zip",root/"fixture-helper.exe",root,std::string(64,'a'),Distribution::portable};
                draw(reader);require(reader.state().at("documents")==before,"Updates UI changed the user document");
                reader.updateOnExit_=reader.preparedUpdate_;
                ImGui::NewFrame();reader.requestQuit();reader.dialogs();
                require(!reader.shouldQuit() && reader.closeId_!=0 && reader.pendingUpdate().has_value(),"Update bypassed dirty-document close guard");
                reader.resolveClose("cancel");ImGui::Render();
                require(!reader.pendingUpdate() && !reader.shouldQuit(),"Cancel did not cancel update handoff");
                require(reader.state().at("documents")==before,"Cancel changed unsaved document");
                reader.updateOnExit_=reader.preparedUpdate_;
                ImGui::NewFrame();reader.requestQuit();reader.dialogs();reader.resolveClose("discard");ImGui::Render();
                require(reader.shouldQuit() && reader.pendingUpdate().has_value(),"Explicit guarded close lost update handoff");
            }
            ImGui::DestroyContext();std::filesystem::remove_all(root);
            std::cout<<"Offline updates UI: EN/FR, dark/light, DPI, unchanged documents and cancelled/confirmed handoff passed without windows or network.\n";return 0;
        } catch (const std::exception &error) {
            std::cerr<<error.what()<<'\n';ImGui::DestroyContext();std::filesystem::remove_all(root);return 1;
        }
    }
};
}
int main(int argc,char **argv) { return argc==2 ? cycode::reader::ReaderUpdateTests::run(cycode::utf8Path(argv[1])) : 1; }
