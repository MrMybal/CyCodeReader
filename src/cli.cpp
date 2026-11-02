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
        if (args.size()<3 || args[1]!="--serve") throw std::runtime_error("Usage: cycodereader_cli --serve DESCRIPTOR [--plugins DIRECTORY] [FILES...]");
        std::filesystem::path plugins;size_t first=3;if (args.size()>4 && args[3]=="--plugins") { plugins=cycode::utf8Path(args[4]);first=5; }
        cycode::reader::Core core(plugins);
        for (size_t i=first;i<args.size();++i) core.add(cycode::reader::Document::open(cycode::utf8Path(args[i])));
        cycode::reader::Bridge bridge(core,cycode::utf8Path(args[2]));std::string line;std::getline(std::cin,line);return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
