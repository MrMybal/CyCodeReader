#pragma once
#include "Core.h"
#include <atomic>
#include <filesystem>
#include <thread>
namespace cycode::reader {
// Private local IPC endpoint. CyToolsCore owns MCP parsing, jobs and identities.
class Bridge {
  public:
    Bridge(Core &, const std::filesystem::path &descriptor);
    ~Bridge();
    Bridge(const Bridge &) = delete;
    Bridge &operator=(const Bridge &) = delete;
    const std::filesystem::path &descriptor() const {
        return descriptor_;
    }

  private:
    void serve();
    void serveClient(std::intptr_t,bool controlOnly=false);
    Core &engine_;
    std::filesystem::path descriptor_;
    std::string token_;
    std::intptr_t listener_ = -1;
    std::atomic<bool> stopping_{false};
    std::thread thread_;
};
} // namespace cycode
