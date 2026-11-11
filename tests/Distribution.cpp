#include "Distribution.h"
#include "Updates.h"
#include "WindowsCommandLine.h"
#include "PortableUpdate.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#include <functional>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

using namespace cycode;using namespace cycode::reader;using Json=nlohmann::json;
namespace {
// Compiler-evaluated checks on the production quoting owner; no executable runs.
using cycode::detail::quoteWindowsArgument;
static_assert(quoteWindowsArgument(L"")==L"\"\"");
static_assert(quoteWindowsArgument(L"plain")==L"\"plain\"");
static_assert(quoteWindowsArgument(L"two words")==L"\"two words\"");
static_assert(quoteWindowsArgument(L"\\")==L"\"\\\\\"");
static_assert(quoteWindowsArgument(L"\\\\")==L"\"\\\\\\\\\"");
static_assert(quoteWindowsArgument(L"C:\\")==L"\"C:\\\\\"");
static_assert(quoteWindowsArgument(L"C:\\Reader Folder\\")==L"\"C:\\Reader Folder\\\\\"");
static_assert(quoteWindowsArgument(L"C:\\Reader Folder\\\\")==L"\"C:\\Reader Folder\\\\\\\\\"");
static_assert(quoteWindowsArgument(L"\\\\host\\share\\")==L"\"\\\\host\\share\\\\\"");
static_assert(quoteWindowsArgument(L"quoted\"value")==L"\"quoted\\\"value\"");
static_assert(quoteWindowsArgument(L"slash\\\"quote")==L"\"slash\\\\\\\"quote\"");
static_assert(quoteWindowsArgument(L"dossier été\\")==L"\"dossier été\\\\\"");

void require(bool condition,const char *message) { if (!condition) throw std::runtime_error(message); }
void refuses(const std::function<void()> &action) { bool failed=false;try { action(); } catch (...) { failed=true; }require(failed,"Unsafe update accepted"); }
void write(const std::filesystem::path &path,const std::string &text) { std::filesystem::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<text; }
void baseline(const std::filesystem::path &root) {
    for (const auto *name : {"CyCodeReader.exe","CyCodeReaderUpdater.exe","portable.flag","locales/ui.json","plugins/obsolete.json"}) write(root/name,"old");
    write(root/"package-files.json",Json{{"version","0.1.0"},{"files",{"CyCodeReader.exe","CyCodeReaderUpdater.exe","portable.flag","locales/ui.json","plugins/obsolete.json","package-files.json"}}}.dump());
    write(root/"data/reader/preferences.json","user preferences");write(root/"plugins/custom.json","user plugin");
}
Json release() {
    return {{"draft",false},{"prerelease",false},{"tag_name","v0.2.0"},{"body","Release notes"},{"assets",Json::array({
        {{"name","CyCodeReader-0.2.0-windows-x64-portable.zip"},{"url","https://api.github.com/repos/"+updateRepository()+"/releases/assets/42"},{"digest","sha256:"+std::string(64,'a')},{"size",128},{"state","uploaded"}},
        {{"name","CyCodeReader-0.2.0-windows-x64-setup.exe"},{"url","https://api.github.com/repos/"+updateRepository()+"/releases/assets/43"},{"digest","sha256:"+std::string(64,'b')},{"size",256},{"state","uploaded"}}
    })}};
}
}
int main(int argc,char **argv) {
    try {
#ifdef _WIN32
        // Parse the exact production handoff through Windows without starting any process.
        for (const auto *directory : {LR"(C:\Users\Fixture User\AppData\Local\Programs\CyCodeReader\)",
                                     LR"(D:\Outils été\CyCodeReader\\)", LR"(C:\)",
                                     LR"(\\server\share\CyCodeReader\)", LR"(D:\Reader without final slash)"}) {
            PreparedUpdate update;
            update.helper=LR"(C:\Users\Fixture User\AppData\Local\CyCodeReader\updates\{fixture}\CyCodeReaderUpdater.exe)";
            update.package=LR"(C:\Users\Fixture User\AppData\Local\CyCodeReader\updates\{fixture}\package setup.exe)";
            update.installation=directory;update.sha256=std::string(64,'a');update.mode=Distribution::installed;
            auto command=updateHelperCommandLine(update,4242);
            int count=0;auto parsed=CommandLineToArgvW(command.c_str(),&count);
            require(parsed!=nullptr,"Cannot parse updater handoff fixture");
            std::vector<std::wstring> actual;for (int i=0;i<count;++i) actual.emplace_back(parsed[i]);LocalFree(parsed);
            const std::vector<std::wstring> expected={update.helper.wstring(),L"--package",update.package.wstring(),
                L"--installation",update.installation.wstring(),L"--sha256",std::wstring(64,L'a'),L"--parent",L"4242"};
            require(actual==expected,"Updater handoff lost a path or swallowed digest/parent arguments");
            refuses([&] { updateHelperCommandLine(update,0); });
            update.sha256="invalid";refuses([&] { updateHelperCommandLine(update,4242); });
            update.sha256=std::string(64,'a');update.installation=L"relative";
            refuses([&] { updateHelperCommandLine(update,4242); });
        }
#endif
        require(argc==2,"Fixture directory required");const auto fixtures=utf8Path(argv[1]);
        require(newerRelease("v1.10.0","1.9.9") && !newerRelease("v0.1.0","0.1.0") && !newerRelease("v0.0.9","0.1.0"),"Numeric version comparison failed");
        for (const auto *version : {"", "1.0", "1.0.0.0", "01.2.3", "1.2.3-beta", "1.2.3+build", "1.2.-3", "65536.0.0"}) refuses([&] { releaseVersion(version); });
        require(packagePathAllowed("fonts/JetBrainsMono-Regular.ttf") && !packagePathAllowed("fonts/unknown.ttf"),"Bundled font package scope failed");
        for (const auto *path : {"../bad", "/root.dll", "data/preferences.json", "app/../../bad", "app/file:stream", "plugins/CON.json", "plugins/a./b.json", "plugins\\bad.json", "plugins/a//b.json", "installed.flag", "runtime/python.exe", "plugins/NUL/x"}) require(!packagePathAllowed(path),"Unsafe package path allowed");
        const auto portable=parseRelease(release(),Distribution::portable);require(portable.asset.apiUrl.ends_with("/42"),"Portable asset selection failed");
        require(parseRelease(release(),Distribution::installed).asset.apiUrl.ends_with("/43"),"Installer asset selection failed");
        auto bad=release();bad["assets"][0]["url"]="https://api.github.com/repos/attacker/other/releases/assets/42";refuses([&] { parseRelease(bad,Distribution::portable); });
        bad=release();bad["assets"][0]["digest"]=nullptr;refuses([&] { parseRelease(bad,Distribution::portable); });
        bad=release();bad["assets"].push_back(bad["assets"][0]);refuses([&] { parseRelease(bad,Distribution::portable); });
        bad=release();bad["prerelease"]=true;refuses([&] { parseRelease(bad,Distribution::portable); });
        bad=release();bad["assets"][0]["size"]=maximumUpdateSize+1;refuses([&] { parseRelease(bad,Distribution::portable); });
        refuses([&] { parseRelease(release(),Distribution::development); });
        const auto root=fixtures/"owned-installation";
        // This fixed fixture belongs to this test, never a product installation.
        std::filesystem::remove_all(root);baseline(root);
        require(readerDataDirectory(root)==root/"data","Portable preferences moved outside installation");
#ifdef _WIN32
        write(root/"hash-input","abc");require(sha256File(root/"hash-input")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 known vector failed");
        {
            InstanceLock first(root/"data"),second(root/"data");
            refuses([&] { InstanceLock exclusive(root/"data",true); });
        }
        {
            InstanceLock exclusive(root/"data",true);
            refuses([&] { InstanceLock another(root/"data"); });
        }
        // Force a real filesystem failure after earlier files moved into backup.
        const auto locked=CreateFileW((root/"locales/ui.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        require(locked!=INVALID_HANDLE_VALUE,"Cannot lock rollback fixture");
        refuses([&] { applyPortableUpdate(fixtures/"valid.zip",root); });CloseHandle(locked);
        require(readTextFileBytes(root/"CyCodeReader.exe")=="old" && readTextFileBytes(root/"plugins/obsolete.json")=="old","Failed update did not roll back");
#endif
        for (const auto *name : {"traversal.zip","data.zip","device.zip","case.zip","compressed.zip","manifest.zip","corrupt.zip"}) {
            refuses([&] { applyPortableUpdate(fixtures/name,root); });
            require(readTextFileBytes(root/"CyCodeReader.exe")=="old","Invalid archive modified installed application");
        }
        applyPortableUpdate(fixtures/"valid.zip",root);
        require(readTextFileBytes(root/"CyCodeReader.exe")=="new application","Portable application was not replaced");
        require(readTextFileBytes(root/"data/reader/preferences.json")=="user preferences" && readTextFileBytes(root/"plugins/custom.json")=="user plugin","Update changed user-owned data");
        require(!std::filesystem::exists(root/"plugins/obsolete.json") && std::filesystem::is_regular_file(root/"plugins/new.json"),"Managed package files were not reconciled");
        refuses([&] { applyPortableUpdate(fixtures/"valid.zip",root); });
        std::filesystem::remove_all(root);
        std::cout<<"Offline distribution checks passed. No GitHub request or application launch.\n";return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
}
