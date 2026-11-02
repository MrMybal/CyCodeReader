#include "PluginABI.h"
#include <nlohmann/json.hpp>
#include <cctype>
#include <string>
static const char *execute(const char *command,const char *input) {
    static std::string result;
    try {
        if (std::string(command)!="wordCount") throw std::runtime_error("Unknown sample command");
        auto data=nlohmann::json::parse(input);const auto text=data.at("text").get<std::string>();size_t count=0;bool word=false;
        for (unsigned char c : text) { const bool next=!std::isspace(c);if (next && !word) ++count;word=next; }
        result=nlohmann::json{{"wordCount",count},{"bytes",text.size()}}.dump();
    } catch (const std::exception &e) { result=nlohmann::json{{"error",e.what()}}.dump(); }
    return result.c_str();
}
#ifdef _WIN32
#define READER_EXPORT __declspec(dllexport)
#else
#define READER_EXPORT __attribute__((visibility("default")))
#endif
extern "C" READER_EXPORT const CyCodeReaderPluginV1 *cycodereader_plugin_v1() {
    static const CyCodeReaderPluginV1 api{CYCODE_READER_PLUGIN_ABI,sizeof(CyCodeReaderPluginV1),execute};return &api;
}
