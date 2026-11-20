#pragma once
#include "cycode/Paths.h"
#include <cyui/modules/Code.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <string>
namespace cycode::reader {
// Stable persisted IDs, never process-local plugin enum values. Unknown IDs survive.
inline std::string syntaxModeId(std::string value) {
    if (value.empty() || value=="auto") return "auto";
    if (value.starts_with("plugin:")) value.erase(0,7);
    if (value=="C++ / Unreal") return "unreal";
    if (value=="TypeScript / TSX") return "tsx";
    if (value=="Plain text") return "plain";
    const auto language=cyui::codeLanguageFromName(value);
    if (language!=cyui::CodeLanguage::plain || value=="plain") return cyui::codeLanguageInfo(language).id;
    return value;
}
inline std::string syntaxPathKey(const std::filesystem::path &path) {
    auto key=pathUtf8(std::filesystem::absolute(path).lexically_normal());
#ifdef _WIN32
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return c>='A'&&c<='Z'?char(c-'A'+'a'):char(c);});
#endif
    return key;
}
class SyntaxPreferences {
    using Json=nlohmann::json;
    Json loaded_=Json::object(),changed_=Json::object();
public:
    void load(const Json &settings) {
        loaded_=settings.contains("syntaxModes")&&settings.at("syntaxModes").is_object()?settings.at("syntaxModes"):Json::object();
        changed_=Json::object();
    }
    std::string choice(const std::filesystem::path &path) const {
        if(path.empty())return "auto";
        const auto key=syntaxPathKey(path);
        const auto read=[&](const Json &items)->std::string {
            auto it=items.find(key);
            if(it==items.end())for(it=items.begin();it!=items.end();++it) {
                try{if(syntaxPathKey(utf8Path(it.key()))==key)break;}catch(...){}
            }
            return it!=items.end()&&it->is_string()?syntaxModeId(it->get<std::string>()):std::string{};
        };
        auto value=read(changed_);if(value.empty())value=read(loaded_);return value.empty()?"auto":value;
    }
    void remember(const std::filesystem::path &path,const std::string &mode) {
        if(!path.empty())changed_[syntaxPathKey(path)]=syntaxModeId(mode);
    }
    void mergeInto(Json &settings) const {
        auto modes=settings.contains("syntaxModes")&&settings.at("syntaxModes").is_object()?settings.at("syntaxModes"):Json::object();
        for(const auto &[key,value]:loaded_.items())if(!modes.contains(key))modes[key]=value;
        for(const auto &[key,value]:changed_.items())modes[key]=value;
        settings["syntaxModes"]=std::move(modes);
    }
};
}
