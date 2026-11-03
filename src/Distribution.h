#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace cycode::reader {
// A single version source is supplied by CMake to every distribution target.
std::string applicationVersion();
std::array<unsigned,3> releaseVersion(std::string_view text);
bool newerRelease(std::string_view candidate,std::string_view current);
enum class Distribution { development, portable, installed };
Distribution distribution(const std::filesystem::path &installation);
std::filesystem::path readerDataDirectory(const std::filesystem::path &installation);
bool packagePathAllowed(std::string_view path);
std::string sha256File(const std::filesystem::path &path);

// A shared file handle lets the updater wait for every instance of this copy.
class InstanceLock {
  public:
    explicit InstanceLock(const std::filesystem::path &dataDirectory,bool exclusive=false,bool installed=false);
    ~InstanceLock();
    InstanceLock(const InstanceLock &)=delete;
    InstanceLock &operator=(const InstanceLock &)=delete;
  private:
    void *handle_=nullptr;
    void *mutex_=nullptr;
};
} // namespace cycode::reader
