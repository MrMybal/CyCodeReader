#include "Updates.h"
#include "PortableUpdate.h"
#include "NativeProcess.h"
#include "cycode/Paths.h"
#include <chrono>
#include <fstream>
#include <memory>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

namespace {
void restart(const std::filesystem::path &installation) {
    const auto executable=installation/L"CyCodeReader.exe";
    std::wstring command=L"\""+executable.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,installation.c_str(),&startup,&process)) throw std::runtime_error("Update installed, but Reader could not restart");
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    using namespace cycode;using namespace cycode::reader;
    std::filesystem::path package,installation;std::string digest;DWORD parent=0;
    try {
        int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
        if (!args) throw std::runtime_error("Cannot read update arguments");
        std::vector<std::wstring> values;for (int i=1;i<count;++i) values.emplace_back(args[i]);LocalFree(args);
        for (size_t i=0;i<values.size();++i) {
            if (i+1==values.size()) throw std::runtime_error("Missing update argument");
            const auto key=values[i++];const auto value=values[i];
            if (key==L"--package") package=value;
            else if (key==L"--installation") installation=value;
            else if (key==L"--sha256") digest=pathUtf8(std::filesystem::path(value));
            else if (key==L"--parent") parent=std::stoul(value);
            else throw std::runtime_error("Unknown update argument");
        }
        if (!parent || !package.is_absolute() || !installation.is_absolute() || digest.size()!=64) throw std::runtime_error("Invalid update arguments");
        const auto mode=distribution(installation);
        if (mode==Distribution::development) throw std::runtime_error("Cannot update a development checkout");
        if (auto process=OpenProcess(SYNCHRONIZE,FALSE,parent)) {
            const auto waited=WaitForSingleObject(process,120000);CloseHandle(process);
            if (waited!=WAIT_OBJECT_0) throw std::runtime_error("Reader did not finish closing; update cancelled");
        } else if (GetLastError()!=ERROR_INVALID_PARAMETER) throw std::runtime_error("Cannot wait for Reader to close");
        std::unique_ptr<InstanceLock> lock;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(2);
        do {
            try { lock=std::make_unique<InstanceLock>(readerDataDirectory(installation),true); } catch (...) { std::this_thread::sleep_for(std::chrono::milliseconds(250)); }
        } while (!lock && std::chrono::steady_clock::now()<deadline);
        if (!lock) throw std::runtime_error("Close all Reader instances using this copy, then retry the update");
        if (sha256File(package)!=digest) throw std::runtime_error("Update SHA-256 verification failed");
        if (mode==Distribution::portable) applyPortableUpdate(package,installation);
        else {
            // Interactive per-user setup; it never forcibly closes another app.
            const auto result=runProcess({pathUtf8(package),"/SP-","/NORESTART","/NOCLOSEAPPLICATIONS","/DIR="+pathUtf8(installation)},installation,std::chrono::minutes(30));
            if (result.exitCode!=0 || result.timedOut) throw std::runtime_error("Installation cancelled or failed. Check the installer log before retrying.");
        }
        lock.reset();restart(installation);return 0;
    } catch (const std::exception &error) {
        if (!package.empty()) std::ofstream(package.parent_path()/"update-error.txt")<<error.what();
        const auto message=cycode::utf8Path(error.what()).wstring();
        MessageBoxW(nullptr,message.c_str(),L"CyCodeReader update",MB_OK|MB_ICONERROR);return 1;
    }
}
#endif
