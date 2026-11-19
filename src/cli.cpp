#include "automation_cli.h"
#include "Core.h"
#include "Bridge.h"
#include "cycode/Paths.h"
#include <iostream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
int main(int argc,char **argv) {
    try {
        std::vector<std::string> args;
#ifdef _WIN32
        int count=0;auto values=CommandLineToArgvW(GetCommandLineW(),&count);
        if (!values) throw std::runtime_error("Cannot read native arguments");
        for (int i=0;i<count;++i) { const auto length=int(wcslen(values[i]));const auto size=WideCharToMultiByte(CP_UTF8,0,values[i],length,nullptr,0,nullptr,nullptr);std::string text(size,0);WideCharToMultiByte(CP_UTF8,0,values[i],length,text.data(),size,nullptr,nullptr);args.push_back(std::move(text)); }LocalFree(values);
#else
        for (int i=0;i<argc;++i) args.emplace_back(argv[i]);
#endif
        return cycode::reader::automationCli(args);
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
