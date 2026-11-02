#pragma once
#include "PluginABI.h"
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
namespace cycode::reader {
using Json=nlohmann::json;
class Plugins {
public:
    ~Plugins();
    void load(const std::filesystem::path &directory);
    Json describe() const;
    Json configure(const std::string &id,const Json &options);
    Json invoke(const std::string &id,const std::string &command,const Json &input);
private:
    struct Plugin { Json manifest,options;void *library=nullptr;const CyCodeReaderPluginV1 *api=nullptr; };
    std::map<std::string,Plugin> plugins_;
    Json errors_=Json::array();
};
std::string transformText(const std::string &,const std::string &operation);
}
