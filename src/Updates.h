#pragma once
#include "Distribution.h"
#include <atomic>
#include <nlohmann/json.hpp>
#include <optional>

namespace cycode::reader {
constexpr uint64_t maximumUpdateSize=256ull*1024*1024;
struct ReleaseAsset { std::string name,apiUrl,sha256;uint64_t size=0; };
struct ReaderRelease { std::string version,notes;ReleaseAsset asset; };
struct PreparedUpdate { std::filesystem::path package,helper,installation;std::string sha256;Distribution mode=Distribution::development; };
std::string updateRepository();
bool publicUpdatesEnabled();
ReaderRelease parseRelease(const nlohmann::json &json,Distribution mode);
std::optional<ReaderRelease> checkForUpdate(Distribution mode,const std::atomic<bool> &cancel);
PreparedUpdate downloadUpdate(const ReaderRelease &,const std::filesystem::path &installation,const std::atomic<bool> &cancel,std::atomic<uint64_t> &progress);
void launchUpdate(const PreparedUpdate &);
} // namespace cycode::reader
