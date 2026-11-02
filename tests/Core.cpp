#include "Core.h"
#include "cycode/Paths.h"
#include <fstream>
#include <chrono>
#include <iostream>
using namespace cycode;using namespace cycode::reader;
static void require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F action,const char *message) { bool failed=false;try { action(); } catch (...) { failed=true; }require(failed,message); }
static void write(const std::filesystem::path &path,const std::string &text) { std::ofstream(path,std::ios::binary)<<text; }
int main(int argc,char **argv) {
    const auto root=std::filesystem::temp_directory_path()/("reader-core-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc==2,"Missing native plugin fixture");std::filesystem::create_directories(root/"plugins");const auto lib=std::filesystem::path(argv[1]);
        std::filesystem::copy_file(lib,root/"plugins"/lib.filename());
        write(root/"plugins/native.json",Json{{"abi",1},{"id","fixture.native"},{"name","Fixture"},{"library","reader_sample_plugin"},{"commands",Json::array({{{"id","wordCount"},{"title","Count"}}})}}.dump());
        write(root/"plugins/data.json",R"({"abi":1,"id":"fixture.data","commands":[{"id":"sort","title":"Sort","operation":"sortLines"}],"languages":[{"id":"config","name":"Fixture config","extensions":[".fixture"],"keywords":["option"]}],"options":[{"id":"enabled","type":"boolean","default":true}]})");
        write(root/"plugins/bad.json",R"({"abi":1,"id":"fixture.bad","library":"../outside"})");
        const auto file=root/utf8Path("été.cpp");write(file,"b\na 🛠\n");
        {
            Core core(root/"plugins");const auto opened=core.dispatch("documents.open",{{"path",pathUtf8(file)}});const auto id=opened.at("id");auto guard=Json{{"id",id},{"expectedRevision",opened.at("revision")}};
            const auto state=core.dispatch("plugins.list");require(state.at("plugins").size()==2 && state.at("errors").size()==1,"Plugin load isolation failed: "+state.dump());
            require(core.dispatch("documents.list").at("documents").size()==1,"Open document absent");
            const auto search=core.dispatch("search.openDocuments",{{"query","🛠"}});require(search.at("matches").size()==1 && search.at("matches")[0].at("line")==2,"Live search failed");
            auto sorted=core.dispatch("plugins.invoke",{{"id",id},{"expectedRevision",opened.at("revision")},{"plugin","fixture.data"},{"command","sort"}});
            require(sorted.at("document").at("text")=="a 🛠\nb\n" && readTextFileBytes(file)=="b\na 🛠\n","Plugin did not preserve unsaved source");
            rejects([&] { core.dispatch("documents.replace",{{"id",id},{"expectedRevision",opened.at("revision")},{"text","obsolete"}}); },"Stale edit accepted");
            auto current=core.dispatch("documents.read",{{"id",id}});guard["expectedRevision"]=current.at("revision");
            const auto words=core.dispatch("plugins.invoke",{{"id",id},{"expectedRevision",current.at("revision")},{"plugin","fixture.native"},{"command","wordCount"}});require(words.at("result").at("wordCount")==3,"Native ABI command did not execute");
            core.dispatch("plugins.configure",{{"plugin","fixture.data"},{"options",{{"enabled",false}}}});
            rejects([&] { core.dispatch("plugins.configure",{{"plugin","fixture.data"},{"options",{{"enabled","wrong"}}}}); },"Wrong plugin option type accepted");
            rejects([&] { core.dispatch("documents.close",guard); },"Dirty close accepted");
            write(file,"// external\n");rejects([&] { core.dispatch("documents.save",guard); },"Disk conflict overwritten");
            require(core.dispatch("documents.read",{{"id",id}}).at("text")==current.at("text"),"Disk conflict damaged live buffer");
            core.documents().at(id.get<size_t>())->readOnly=true;
            rejects([&] { core.dispatch("text.transform",{{"id",id},{"expectedRevision",current.at("revision")},{"operation","uppercase"}}); },"Read-only transform accepted");
            core.documents().at(id.get<size_t>())->readOnly=false;guard["discard"]=true;core.dispatch("documents.close",guard);
            rejects([&] { core.dispatch("documents.read",{{"id",id}}); },"Closed document remained readable");
            require(transformText("é ab 🛠","uppercase")=="é AB 🛠","ASCII conversion damaged Unicode");
            require(transformText("a \n\tb\t\n","trimTrailing")=="a\n\tb\n","Whitespace transformation failed");
            require(transformText("a\nb\na","uniqueLines")=="a\nb","Duplicate removal changed final newline");
        }
        std::filesystem::remove_all(root);std::cout<<"Reader core: actual native/data plugins, languages/options, live revisions/search, unsaved edits and disk guards passed\n";return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<"\nArtifacts: "<<root<<'\n';return 1; }
}
