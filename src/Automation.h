#pragma once
#include "Core.h"
#include <cymcp/mcp.hpp>
#include <cymcp/http.hpp>
#include <cymcp/changes.hpp>
#include <cymcp/performance.hpp>
#include <future>
#include <map>
namespace cycode::reader {
const char* accessProfileName(cymcp::LocalAccessProfile);
struct AutomationOptions {
    cymcp::LocalAccessProfile accessProfile=cymcp::LocalAccessProfile::free;
    bool compact=true,resources=true,notifications=true,metrics=false,diskResults=true,allowPlugins=false,namedCopies=true;
    size_t inlineBytes=16384,resultMemoryBytes=64*1024*1024,resultDiskBytes=512*1024*1024;
    int resultTtlSeconds=300,cacheTtlSeconds=30;
    size_t durableJobBytes=512*1024*1024,maxDurableJobs=1024;
    static AutomationOptions fromJson(const Json&);
};
class Automation {
public:
    using Dispatch=std::function<Json(const std::string&,const Json&,const cymcp::CallContext&)>;
    Automation(std::filesystem::path data,Dispatch,bool gui,AutomationOptions={});
    ~Automation();
    std::shared_ptr<cymcp::Registry> registry() const{return registry_;}
    std::shared_ptr<cymcp::Registry> view(const std::string &principal) const;
    std::shared_ptr<const cymcp::Registry> tools(const std::string &principal,bool compact) const;
    std::shared_ptr<cymcp::ResourceRegistry> resources() const{return resources_;}
    std::unique_ptr<cymcp::McpServer> mcp(std::shared_ptr<const cymcp::Registry>) const;
    Json invoke(std::string name,const Json&,std::string principal="read",const cymcp::CallContext& = {});
    Json request(const Json&,const cymcp::CallContext&);
    void start(const std::filesystem::path &descriptor);
    void stop();
    void changed();
    void connectRemotes(const Json &configured,const cymcp::Cancellation& = {});
    Json status() const;
    const std::filesystem::path& dataDirectory() const{return data_;}
private:
    Json perform(const std::string&,const std::string&,const Json&,const cymcp::CallContext&);
    bool allowed(const std::string&,const cymcp::CallContext&) const;
    Json jobs(const Json&,const cymcp::CallContext&);
    void writeJob(const Json&);
    void observeDocument(const Json&);
    void reserveJob() const;
    Json execute(const std::string&,const Json&,const cymcp::CallContext&,const std::function<Json(const cymcp::CallContext&)>&);
    struct ProfileLease;
    std::unique_ptr<ProfileLease> lease_;
    std::filesystem::path data_,descriptor_;
    Dispatch dispatch_;AutomationOptions options_;bool gui_;
    std::shared_ptr<cymcp::Registry> registry_;
    std::shared_ptr<cymcp::ResourceRegistry> resources_;
    std::shared_ptr<cymcp::ChangeFeed> changes_;
    std::shared_ptr<cymcp::PerformanceMonitor> performance_;
    std::map<std::string,Json> contracts_;
    mutable std::recursive_mutex contractsMutex_;
    std::mutex resourceMutex_;
    std::map<std::string,size_t> documentResources_;
    mutable std::mutex jobsMutex_;
    std::map<std::string,cymcp::Cancellation> activeJobs_;
    std::vector<std::shared_ptr<cymcp::McpClient>> remotes_;
    std::unique_ptr<cymcp::LocalHttpServer> server_;
    std::shared_ptr<const cymcp::Registry> serverTools_;
    std::unique_ptr<cymcp::McpServer> serverMcp_;
    std::string readToken_,editToken_,profileId_,published_;
    bool ownsDescriptor_=false;
};
Json legacyEndpointRequest(const std::filesystem::path&,const std::string&,const Json&,const cymcp::CallContext&);
Json dispatchTextSession(Core&,const std::string&,const Json&,const cymcp::CallContext&);
}
