#include "Automation.h"
#include "ReaderOperations.h"
#include "cycode/Error.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif
namespace cycode::reader {
namespace {
Json object(){return {{"type","object"}};}
bool safeId(const std::string &id){return !id.empty() && id.size()<=96 && std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_';});}
Json load(const std::filesystem::path &path,size_t max=64*1024*1024){
    if(std::filesystem::is_symlink(path) || std::filesystem::file_size(path)>max)throw cymcp::Error("InvalidData","Redirected or oversized private data");
    std::ifstream in(path,std::ios::binary);if(!in)throw cymcp::Error("FileError","Cannot read private data");
    return cymcp::parse_json(std::string(std::istreambuf_iterator<char>(in),{}),max);
}
void write(const std::filesystem::path &path,const Json &value){
    const auto temporary=path.parent_path()/("receipt-"+cymcp::random_token(16)+".tmp");
    try{
        auto output=cymcp::open_private_output_file(temporary);const auto bytes=value.dump();
        if(std::fwrite(bytes.data(),1,bytes.size(),output.get())!=bytes.size()||std::fflush(output.get()))throw cymcp::Error("FileError","Cannot persist job receipt");
#ifdef _WIN32
        if(!FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(output.get())))))throw cymcp::Error("FileError","Cannot flush job receipt");
        output.reset();
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw cymcp::Error("FileError","Cannot commit job receipt");
#else
        if(::fsync(fileno(output.get())))throw cymcp::Error("FileError","Cannot flush job receipt");output.reset();
        std::filesystem::rename(temporary,path);const int directory=::open(path.parent_path().c_str(),O_RDONLY|O_DIRECTORY);
        if(directory>=0){::fsync(directory);::close(directory);}
#endif
    }catch(...){std::error_code error;std::filesystem::remove(temporary,error);throw;}
}
bool editor(const std::string &principal){return principal=="edit"||principal=="chat-edit";}
}
struct Automation::ProfileLease {
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
    explicit ProfileLease(const std::filesystem::path &p){handle=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(handle==INVALID_HANDLE_VALUE)throw cymcp::Error("ProfileBusy","This automation profile is already in use; select another --data-dir");}
    ~ProfileLease(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
#else
    int handle=-1;
    explicit ProfileLease(const std::filesystem::path &p){handle=::open(p.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW,0600);if(handle<0||::flock(handle,LOCK_EX|LOCK_NB)){if(handle>=0)::close(handle);throw cymcp::Error("ProfileBusy","This automation profile is already in use");}}
    ~ProfileLease(){if(handle>=0){::flock(handle,LOCK_UN);::close(handle);}}
#endif
};
const char* accessProfileName(cymcp::LocalAccessProfile profile){
    switch(profile){
    case cymcp::LocalAccessProfile::free:return "free";
    case cymcp::LocalAccessProfile::local_controlled:return "local_controlled";
    case cymcp::LocalAccessProfile::token_only:return "token_only";
    case cymcp::LocalAccessProfile::protected_access:return "protected";
    }
    throw cymcp::Error("InvalidOptions","Unknown local MCP access profile");
}
AutomationOptions AutomationOptions::fromJson(const Json &j){
    AutomationOptions o;
    const auto parseProfile=[](const Json &value){
        if(!value.is_string())throw cymcp::Error("InvalidOptions","MCP access profile must be a string");
        const auto name=value.get<std::string>();
        if(name=="free")return cymcp::LocalAccessProfile::free;
        if(name=="local_controlled")return cymcp::LocalAccessProfile::local_controlled;
        if(name=="token_only")return cymcp::LocalAccessProfile::token_only;
        if(name=="protected"||name=="protected_access")return cymcp::LocalAccessProfile::protected_access;
        throw cymcp::Error("InvalidOptions","Unknown local MCP access profile");
    };
    if(j.contains("accessProfile"))o.accessProfile=parseProfile(j.at("accessProfile"));
    if(j.contains("access_profile")){
        const auto profile=parseProfile(j.at("access_profile"));
        if(j.contains("accessProfile")&&profile!=o.accessProfile)throw cymcp::Error("InvalidOptions","Conflicting MCP access profiles");
        o.accessProfile=profile;
    }
    o.compact=j.value("compact",o.compact);o.resources=j.value("resources",o.resources);
    o.notifications=j.value("notifications",o.notifications);o.metrics=j.value("metrics",o.metrics);
    o.diskResults=j.value("diskResults",o.diskResults);o.allowPlugins=j.value("allowPlugins",o.allowPlugins);
    o.inlineBytes=j.value("inlineBytes",o.inlineBytes);o.resultMemoryBytes=j.value("resultMemoryBytes",o.resultMemoryBytes);
    o.resultDiskBytes=j.value("resultDiskBytes",o.resultDiskBytes);o.resultTtlSeconds=j.value("resultTtlSeconds",o.resultTtlSeconds);
    o.cacheTtlSeconds=j.value("cacheTtlSeconds",o.cacheTtlSeconds);
    o.durableJobBytes=j.value("durableJobBytes",o.durableJobBytes);o.maxDurableJobs=j.value("maxDurableJobs",o.maxDurableJobs);
    if(o.durableJobBytes<64*1024*1024||!o.maxDurableJobs||o.maxDurableJobs>100000)throw cymcp::Error("InvalidOptions","Invalid durable receipt budget");
    if(o.inlineBytes<1024||o.inlineBytes>1024*1024||o.resultTtlSeconds<1||o.cacheTtlSeconds<0||o.cacheTtlSeconds>3600)
        throw cymcp::Error("InvalidOptions","Invalid automation optimization budget");
    return o;
}
Automation::Automation(std::filesystem::path data,Dispatch dispatch,bool gui,AutomationOptions options)
    :data_(std::filesystem::absolute(data).lexically_normal()),dispatch_(std::move(dispatch)),options_(options),gui_(gui){
    cymcp::private_directory(data_);lease_=std::make_unique<ProfileLease>(data_/"profile.lock");cymcp::private_directory(data_/"jobs");
    const auto identity=data_/"profile-id.json";
    if(std::filesystem::exists(identity))profileId_=load(identity,4096).at("id").get<std::string>();
    else{profileId_=cymcp::random_token(16);write(identity,Json{{"id",profileId_}});}
    if(options_.notifications)changes_=std::make_shared<cymcp::ChangeFeed>();
    if(options_.metrics){cymcp::PerformanceOptions p;p.jsonl_file=data_/"performance.jsonl";performance_=std::make_shared<cymcp::PerformanceMonitor>(p);}
    const auto policy=cymcp::local_access_settings(options_.accessProfile).control_operations
        ? cymcp::Policy([this](const auto &c,const auto &n,const auto &){return allowed(n,c)?cymcp::Decision::allow:cymcp::Decision::deny;})
        : cymcp::Policy::allow_all();
    registry_=std::make_shared<cymcp::Registry>(policy);
    registry_->set_change_feed(changes_);registry_->set_performance_monitor(performance_);
    cymcp::ResourceOptions storage;storage.chunk_bytes=32768;storage.max_stored_bytes=options_.resultMemoryBytes;
    storage.result_ttl=std::chrono::seconds(options_.resultTtlSeconds);storage.max_disk_bytes=options_.resultDiskBytes;
    if(options_.diskResults)storage.disk_directory=data_/"results";
    resources_=std::make_shared<cymcp::ResourceRegistry>(cymcp::Policy::allow_all(),storage);resources_->set_change_feed(changes_);
    const auto catalogue=Json::parse(readerOperationManifest);
    for(const auto &spec:catalogue.at("operations")){
        if(spec.value("guiOnly",false)&&!gui_)continue;
        if(!options_.namedCopies&&spec.at("id")=="documents_save_as")continue;
        const auto name=spec.at("id").get<std::string>();contracts_[name]=spec;
        cymcp::Operation op;op.name=name;op.description=spec.at("description");
        op.input_schema=spec.at("inputSchema");op.output_schema=spec.at("outputSchema");op.read_only=spec.value("readOnly",spec.at("permissions").empty());
        const auto method=spec.at("nativeMethod").get<std::string>();
        op.handler=[this,name,method](const Json &p,const cymcp::CallContext &c){return perform(name,method,p,c);};registry_->add(std::move(op));
    }
    // Previously interrupted actions are outcomes to reconcile, never work to replay.
    for(const auto &entry:std::filesystem::directory_iterator(data_/"jobs"))if(entry.path().extension()==".json"){
        try{auto record=load(entry.path());if(record.value("status","")=="running"){record["status"]="interrupted-unknown";writeJob(record);}}catch(...){}
    }
}
Automation::~Automation(){stop();}
bool Automation::allowed(const std::string &name,const cymcp::CallContext &c) const{
    std::lock_guard lock(contractsMutex_);
    const auto it=contracts_.find(name);if(it==contracts_.end())return false;
    for(const auto &permission:it->second.at("permissions")){
        if(!editor(c.principal))return false;
        if(permission=="plugins.execute"&&!options_.allowPlugins)return false;
    }
    return c.principal=="read"||c.principal=="edit"||c.principal=="chat-read"||c.principal=="chat-edit";
}
std::shared_ptr<cymcp::Registry> Automation::view(const std::string &principal) const{
    std::lock_guard lock(contractsMutex_);
    auto result=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
    for(const auto &description:registry_->describe()){
        const auto name=description.at("name").get<std::string>();cymcp::Operation op;
        op.name=name;op.description=description.at("description");op.input_schema=description.at("inputSchema");
        op.output_schema=description.value("outputSchema",object());op.read_only=contracts_.at(name).value("readOnly",contracts_.at(name).at("permissions").empty());
        op.handler=[this,principal,name](const Json &p,const cymcp::CallContext &context){auto c=context;c.principal=principal;
            // Chat permissions are an independent explicit user choice.
            if(principal.starts_with("chat-")&&!allowed(name,c))throw cymcp::Error("AccessDenied","Chat document editing is disabled");
            return registry_->invoke(name,p,c);};
        result->add(std::move(op));
    }
    result->set_change_feed(changes_);return result;
}
std::shared_ptr<const cymcp::Registry> Automation::tools(const std::string &principal,bool compact) const{
    auto result=view(principal);
    if(!compact)return result;
    cymcp::ToolGatewayOptions options;options.inline_bytes=options_.inlineBytes;
    auto gateway=std::make_shared<cymcp::ToolGateway>(result,options,resources_)->tools();
    auto bound=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
    for(const auto &spec:gateway->describe()){
        cymcp::Operation op;op.name=spec.at("name");op.description=spec.at("description");op.input_schema=spec.at("inputSchema");op.output_schema=spec.value("outputSchema",object());
        const auto name=op.name;op.handler=[gateway,principal,name](const Json &p,const cymcp::CallContext &input){auto c=input;c.principal=principal;return gateway->invoke(name,p,c);};bound->add(std::move(op));
    }
    bound->set_change_feed(changes_);return bound;
}
std::unique_ptr<cymcp::McpServer> Automation::mcp(std::shared_ptr<const cymcp::Registry> tools) const{
    cymcp::McpServerOptions o;o.cache_ttl=std::chrono::seconds(options_.cacheTtlSeconds);o.changes=changes_;
    if(options_.resources)o.resources=resources_;o.performance=performance_;
    return std::make_unique<cymcp::McpServer>(*tools,"CyCodeReader",o);
}
Json Automation::invoke(std::string name,const Json &p,std::string principal,const cymcp::CallContext &context){
    if(name.starts_with("op_"))name.erase(0,3);auto c=context;c.principal=std::move(principal);
    if(c.request_id.empty())c.request_id=cymcp::random_token(16);
    return registry_->invoke(name,p,c);
}
void Automation::writeJob(const Json &job){
    const auto id=job.at("id").get<std::string>();if(!safeId(id))throw cymcp::Error("InvalidParameters","Invalid job identity");
    write(data_/"jobs"/(id+".json"),job);
}
void Automation::reserveJob() const{
    size_t bytes=0,count=0;for(const auto &e:std::filesystem::directory_iterator(data_/"jobs"))if(e.is_regular_file()){bytes+=e.file_size();++count;}
    // Reserve a bounded ordinary-document result before any business action.
    constexpr size_t reserve=64*1024*1024;
    if(count>=options_.maxDurableJobs||bytes>options_.durableJobBytes||reserve*(activeJobs_.size()+1)>options_.durableJobBytes-bytes)
        throw cymcp::Error("StorageFull","Durable receipt budget exhausted; inspect then explicitly forget old jobs or increase durableJobBytes");
}
Json Automation::jobs(const Json &p,const cymcp::CallContext &c){
    std::lock_guard lock(jobsMutex_);
    if(p.contains("id")){
        const auto id=p.at("id").get<std::string>();if(!safeId(id))throw cymcp::Error("InvalidParameters","Invalid job identity");
        auto record=load(data_/"jobs"/(id+".json"));
        if(record.at("owner")!=c.principal)throw cymcp::Error("AccessDenied","Job belongs to a different identity");
        if(p.value("forget",false)){
            if(!p.value("confirm",false))throw cymcp::Error("ConfirmationRequired","confirm=true is required to forget a receipt");
            if(activeJobs_.contains(id))throw cymcp::Error("Busy","An active receipt cannot be forgotten");
            std::filesystem::remove(data_/"jobs"/(id+".json"));return {{"forgotten",id}};
        }
        if(p.value("cancel",false)){auto it=activeJobs_.find(id);if(it!=activeJobs_.end())it->second.cancel();return {{"id",id},{"cancellationRequested",it!=activeJobs_.end()}};}
        return record;
    }
    std::vector<Json> items;
    for(const auto &entry:std::filesystem::directory_iterator(data_/"jobs"))if(entry.path().extension()==".json"){
        auto job=load(entry.path());if(job.value("owner","")==c.principal){job.erase("result");job.erase("legacy");items.push_back(std::move(job));}
    }
    std::sort(items.begin(),items.end(),[](const auto&a,const auto&b){return a.value("created",0LL)>b.value("created",0LL);});
    const auto offset=p.value("offset",size_t(0)),limit=std::min(p.value("limit",size_t(50)),size_t(200));Json result=Json::array();
    for(size_t i=offset;i<items.size()&&i-offset<limit;++i)result.push_back(items[i]);
    return {{"jobs",result},{"total",items.size()},{"nextOffset",offset+result.size()}};
}
void Automation::observeDocument(const Json &value){
    if(!options_.resources||!value.contains("text")||!value.contains("id"))return;
    auto bytes=std::make_shared<const std::string>(value.at("text").get<std::string>());
    const auto uri="reader://documents/"+std::to_string(value.at("id").get<size_t>())+"/"+std::to_string(value.at("revision").get<size_t>());
    cymcp::Resource r;r.uri=uri;r.name="Live document "+std::to_string(value.at("id").get<size_t>());
    r.description="Snapshot of an explicitly read live document; reread its operation after a change notification.";r.size=bytes->size();
    r.read=[bytes](size_t offset,size_t length,const auto &){return offset>=bytes->size()?std::string{}:bytes->substr(offset,length);};
    std::lock_guard lock(resourceMutex_);
    if(bytes->size()>options_.resultMemoryBytes)return;
    size_t retained=bytes->size();for(const auto &[key,size]:documentResources_)if(key!=uri)retained+=size;
    while(retained>options_.resultMemoryBytes&&!documentResources_.empty()){
        auto old=documentResources_.begin();if(old->first!=uri)retained-=old->second;resources_->remove(old->first);documentResources_.erase(old);
    }
    if(documentResources_.contains(uri))resources_->replace(std::move(r));else resources_->add(std::move(r));documentResources_[uri]=bytes->size();
}
Json Automation::perform(const std::string &name,const std::string &method,const Json &p,const cymcp::CallContext &incoming){
    if(method=="automation.status")return status();
    if(method=="jobs.list"||method=="jobs.get"||method=="jobs.cancel"||method=="jobs.forget")return jobs(p,incoming);
    if(method=="resources.read")return resources_->read(p.at("uri"),incoming);
    if(method=="resources.grep")return resources_->grep(p.at("query"),p.value("ignoreCase",false),p.value("cursor",""),incoming);
    if(method=="automation.metrics")return performance_?performance_->snapshot():Json{{"enabled",false}};
    return execute(name,p,incoming,[&](const auto &context){return dispatch_(method,p,context);});
}
Json Automation::execute(const std::string &name,const Json &p,const cymcp::CallContext &incoming,const std::function<Json(const cymcp::CallContext&)> &work){
    auto c=incoming;c.cancellation=incoming.cancellation.child();c.cancellation.check();
    const auto id=cymcp::random_token(16);
    Json job={{"id",id},{"owner",c.principal},{"operation",name},{"requestId",c.request_id},{"status","running"},
        {"created",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()}};
    {std::lock_guard lock(jobsMutex_);reserveJob();writeJob(job);activeJobs_[id]=c.cancellation;}
    try{
        const auto result=work(c);
        // Persist the authoritative outcome before returning; cancellation after a
        // mutation may cancel the transport response, but never loses this receipt.
        job["status"]="completed";job["result"]=result;
        {std::lock_guard lock(jobsMutex_);writeJob(job);activeJobs_.erase(id);}
        // Notification/resource publication is secondary to the committed receipt.
        try{bool mutates;{std::lock_guard lock(contractsMutex_);mutates=!contracts_.at(name).value("readOnly",false);}if(mutates)changed();observeDocument(result);}catch(...){}
        return result;
    }catch(const std::exception &e){
        job["status"]=c.cancellation.cancelled()?"cancelled":"failed";
        job["error"]={{"message",cymcp::redact_error_message(e.what())}};
        {std::lock_guard lock(jobsMutex_);writeJob(job);activeJobs_.erase(id);}
        if(const auto native=dynamic_cast<const NativeError*>(&e))throw cymcp::Error(errorCodeName(native->code()),native->what());
        if(dynamic_cast<const cymcp::Error*>(&e))throw;
        throw cymcp::Error("OperationFailed",e.what());
    }
}
Json Automation::request(const Json &message,const cymcp::CallContext &c){
    if(message.contains("jsonrpc"))return serverMcp_->handle(message,c);
    return cymcp::dispatch_app(*registry_,message,c);
}
void Automation::start(const std::filesystem::path &descriptor){
    if(server_)return;descriptor_=std::filesystem::absolute(descriptor);
    if(!std::filesystem::exists(descriptor_.parent_path()))cymcp::private_directory(descriptor_.parent_path());
    if(std::filesystem::exists(descriptor_)){
        const auto previous=load(descriptor_,16384);
        if(previous.value("protocol","")!="cycodereader-cymcp/1"||previous.value("profileId","")!=profileId_)
            throw cymcp::Error("DescriptorBusy","Preserve an existing descriptor belonging to another profile; choose a separate path");
    }
    readToken_.clear();editToken_.clear();
    cymcp::ToolGatewayOptions gateway;gateway.inline_bytes=options_.inlineBytes;
    serverTools_=options_.compact?std::make_shared<cymcp::ToolGateway>(registry_,gateway,resources_)->tools():registry_;
    serverMcp_=mcp(serverTools_);
    const auto access=cymcp::local_access_settings(options_.accessProfile);
    const auto endpoint=[this](const Json&m,const cymcp::CallContext&incoming){
        auto c=incoming;
        // An address-only controlled caller receives the read identity, never JSON-supplied rights.
        if(c.principal=="unauthenticated-local")c.principal="read";
        return request(m,c);
    };
    if(access.require_token){
        readToken_=cymcp::random_token();editToken_=cymcp::random_token();
        server_=std::make_unique<cymcp::LocalHttpServer>(cymcp::TokenAuthenticator([this](std::string_view token){
            if(cymcp::constant_time_equal(std::string(token),readToken_))return std::string("read");
            if(cymcp::constant_time_equal(std::string(token),editToken_))return std::string("edit");
            throw cymcp::Error("AuthenticationRequired","Invalid Reader credential");
        }),endpoint,4,20*1024*1024);
    }else server_=std::make_unique<cymcp::LocalHttpServer>(cymcp::LocalHttpServer::UnauthenticatedLoopback{},endpoint,4,20*1024*1024);
    const auto port=server_->start();
    auto value=Json{{"protocol","cycodereader-cymcp/1"},{"profileId",profileId_},
        {"url","http://127.0.0.1:"+std::to_string(port)+"/v1"},{"mcpUrl","http://127.0.0.1:"+std::to_string(port)+"/mcp"},
        {"accessProfile",accessProfileName(options_.accessProfile)},{"requireToken",access.require_token},
        {"controlOperations",access.control_operations},{"sdkVersion",cymcp::version}};
    if(access.require_token){value["readToken"]=readToken_;value["editToken"]=editToken_;}
    write(descriptor_,value);published_=value.dump();ownsDescriptor_=true;
}
void Automation::stop(){
    {std::lock_guard lock(jobsMutex_);for(const auto &[id,token]:activeJobs_)token.cancel();}
    if(server_){server_->stop();server_.reset();}
    if(ownsDescriptor_){
        try{if(load(descriptor_,16384).dump()==published_){std::error_code ec;std::filesystem::remove(descriptor_,ec);}}catch(...){}
        ownsDescriptor_=false;
    }
    descriptor_.clear();
}
void Automation::changed(){
    {std::lock_guard lock(resourceMutex_);for(const auto &[uri,size]:documentResources_)resources_->remove(uri);documentResources_.clear();}
    if(changes_)changes_->publish("resources");
}
Json Automation::status() const{return {{"sdk","CyMCPSDK"},{"version",cymcp::version},{"sourceRevision",CYCODE_READER_CYMCP_REVISION},
    {"sourceDigest",CYCODE_READER_CYMCP_DIGEST},{"accessProfile",accessProfileName(options_.accessProfile)},
    {"requireToken",cymcp::local_access_settings(options_.accessProfile).require_token},
    {"controlOperations",cymcp::local_access_settings(options_.accessProfile).control_operations},{"operations",registry_->size()},{"gui",gui_},{"compact",options_.compact},
    {"resources",options_.resources},{"notifications",options_.notifications},{"metrics",options_.metrics},{"pluginsAllowed",!cymcp::local_access_settings(options_.accessProfile).control_operations||options_.allowPlugins},{"chatPluginsAllowed",options_.allowPlugins},
    {"pagedAutomation",false},{"durableJobResults",true},{"resourceStorage",resources_->storage_stats()},{"durableJobBytes",options_.durableJobBytes},{"maxDurableJobs",options_.maxDurableJobs}};}
Json dispatchTextSession(Core &core,const std::string &method,const Json &p,const cymcp::CallContext &c){
    c.cancellation.check();std::lock_guard lock(core.mutex());c.cancellation.check();
    if(method=="documents.saveAs"){
        core.refresh();auto found=core.documents().find(p.at("id").get<size_t>());
        if(found==core.documents().end())throw cymcp::Error("NotFound","Document is no longer open");
        auto &live=*found->second;
        if(live.revision!=p.at("expectedRevision"))throw cymcp::Error("RevisionConflict","Read the live document again");
        if(live.readOnly)throw cymcp::Error("ReadOnly","Document is read-only");
        const auto path=utf8Path(p.at("path"));
        if(std::filesystem::exists(path))throw cymcp::Error("ConfirmationRequired","Save As refuses an existing destination; use the GUI confirmation flow");
        for(const auto &[id,other]:core.documents())if(id!=live.id&&sameFile(other->document.path(),path))throw cymcp::Error("Conflict","Destination is already open");
        live.document.saveAs(path,std::nullopt);core.refresh();return core.dispatch("documents.read",{{"id",live.id}});
    }
    return core.dispatch(method,p);
}
}
