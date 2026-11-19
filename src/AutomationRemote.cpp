#include "Automation.h"
#include <cstdlib>
#include <algorithm>
#include <cctype>
namespace cycode::reader {
void Automation::connectRemotes(const Json &configured,const cymcp::Cancellation &cancel){
    for(const auto &entry:configured){
        cancel.check();const auto alias=entry.at("id").get<std::string>();
        if(alias.empty()||alias.size()>32||!std::all_of(alias.begin(),alias.end(),[](unsigned char c){return std::isalnum(c)||c=='_';}))
            throw cymcp::Error("InvalidOptions","Remote MCP identity must be portable ASCII");
        std::shared_ptr<cymcp::McpClient> client;
        if(entry.contains("url")){
            const auto env=entry.value("tokenEnv",std::string{});const auto token=env.empty()?nullptr:std::getenv(env.c_str());
            client=std::make_shared<cymcp::McpClient>(entry.at("url"),token?token:"");
        }else{
            cymcp::ProcessOptions p;p.executable=entry.at("command").get<std::string>();
            p.arguments=entry.value("args",std::vector<std::string>{});p.cwd=data_;client=std::make_shared<cymcp::McpClient>(p);
        }
        client->connect(cancel);const auto catalogue=client->list_tools(cancel);
        for(const auto &name:entry.value("allowTools",std::vector<std::string>{})){
            const auto found=std::find_if(catalogue.begin(),catalogue.end(),[&](const auto &tool){return tool.at("name")==name;});
            if(found==catalogue.end())throw cymcp::Error("UnknownOperation","Allowlisted remote MCP tool was not advertised");
            cymcp::Operation op;op.name="remote."+alias+"."+name;op.description=found->value("description",name);
            op.input_schema=found->at("inputSchema");op.output_schema={{"type","object"}};op.read_only=false;
            const auto identity=op.name;
            op.handler=[this,client,name,identity](const Json &p,const cymcp::CallContext &c){
                return execute(identity,p,c,[&](const auto &context){
                    const auto result=client->call_tool(name,p,context.cancellation);
                    if(result.value("isError",false))throw cymcp::Error("RemoteToolFailed","The remote MCP tool reported an error: "+cymcp::redact_error_message(result.value("content",Json::array()).dump().substr(0,2048)));
                    return Json{{"result",result}};
                });
            };
            std::lock_guard lock(contractsMutex_);contracts_[op.name]={{"permissions",{"remote.execute"}},{"readOnly",false}};
            // Remote handlers still pass through canonical permission checks.
            registry_->add(std::move(op));
        }
        remotes_.push_back(std::move(client));
    }
}
}
