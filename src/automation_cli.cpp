#include "automation_cli.h"
#include "Automation.h"
#include "Bridge.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <thread>
namespace cycode::reader {
namespace {
Json readJson(const std::filesystem::path &p){
    if(std::filesystem::file_size(p)>20*1024*1024)throw cymcp::Error("InvalidParameters","JSON input exceeds 20 MiB");
    std::ifstream in(p,std::ios::binary);if(!in)throw cymcp::Error("FileError","Cannot read JSON input");
    return cymcp::parse_json(std::string(std::istreambuf_iterator<char>(in),{}),20*1024*1024);
}
Json remoteRequest(const Json &descriptor,const std::string &access,const Json &message,const cymcp::Cancellation &cancel={}){
    if(descriptor.value("protocol","")!="cycodereader-cymcp/1")throw cymcp::Error("UnsupportedDescriptor","Copy a current Reader CyMCPSDK connection descriptor");
    const auto url=descriptor.at("url").get<std::string>();const auto parsed=cymcp::parse_url(url);
    if(!parsed.loopback||parsed.scheme!="http")throw cymcp::Error("InvalidDescriptor","Reader instance must use authenticated loopback HTTP");
    cymcp::HttpRequest request;request.method="POST";request.url=url;request.body=message.dump();
    request.headers={{"Authorization","Bearer "+descriptor.at(access=="edit"?"editToken":"readToken").get<std::string>()},{"Content-Type","application/json"}};
    request.max_response_bytes=64*1024*1024;
    const auto reply=cymcp::HttpClient().perform(request,cancel);auto result=cymcp::parse_json(reply.body,64*1024*1024);
    if(result.contains("error"))throw cymcp::Error(result["error"].value("code","RemoteError"),result["error"].value("message","Reader rejected the request"));
    if(reply.status!=200)throw cymcp::Error("RemoteError","Reader service rejected the request");return result;
}
}
int automationCli(const std::vector<std::string> &args){
    try{
        if(args.size()>2&&args[1]=="--serve"){
            // Original raw native bridge mode remains byte-for-byte compatible.
            std::filesystem::path plugins;size_t first=3;
            if(args.size()>4&&args[3]=="--plugins"){plugins=utf8Path(args[4]);first=5;}
            Core core(plugins);for(size_t i=first;i<args.size();++i)core.add(Document::open(utf8Path(args[i])));
            Bridge bridge(core,utf8Path(args[2]));std::string line;std::getline(std::cin,line);return 0;
        }
        std::string access="read",connect,descriptor,config,jsonInput,jsonFile,data,plugins,native;bool full=false,legacy=false;
        std::vector<std::string> files,positional;
        for(size_t i=1;i<args.size();++i){
            const auto next=[&](){if(i+1>=args.size())throw cymcp::Error("InvalidArguments","Missing option value");return args[++i];};
            if(args[i]=="--connect"||args[i]=="--bridge-config")connect=next();
            else if(args[i]=="--native-descriptor")native=next();
            else if(args[i]=="--desktop-service"){legacy=true;positional={"serve"};}
            else if(args[i]=="--access")access=next();
            else if(args[i]=="--descriptor"||args[i]=="--service-descriptor")descriptor=next();
            else if(args[i]=="--data-dir")data=next();else if(args[i]=="--config")config=next();
            else if(args[i]=="--plugins")plugins=next();else if(args[i]=="--file")files.push_back(next());
            else if(args[i]=="--json")jsonInput=next();else if(args[i]=="--json-file")jsonFile=next();
            else if(args[i]=="--full-tools")full=true;else if(args[i]=="--help"||args[i]=="-h")positional={"help"};
            else if(args[i].starts_with('-'))throw cymcp::Error("InvalidArguments","Unknown Reader CLI option");
            else positional.push_back(args[i]);
        }
        const auto command=positional.empty()?std::string("help"):positional[0];
        if(command=="help"){
            std::cout<<"CyCodeReader native CLI\n"
              "  list | describe [OPERATION] | invoke OPERATION --json JSON | jsonl | serve | mcp\n"
              "  --connect DESCRIPTOR --access read|edit  (the actual running GUI instance)\n"
              "  --file FILE --data-dir DIRECTORY        (isolated headless text session)\n"
              "  --json-file FILE --config FILE --plugins DIRECTORY --full-tools\n"
              "  serve --descriptor FILE                (authenticated loopback MCP/JSON)\n"
              "  --desktop-service --native-descriptor FILE --service-descriptor FILE (legacy SDK host)\n"
              "  --serve FILE [--plugins DIRECTORY] [FILES...] (original native bridge)\n"
              "Edits never save implicitly. Use jsonl/serve or --connect for multi-call revisions.\n";return 0;
        }
        if(access!="read"&&access!="edit")throw cymcp::Error("InvalidArguments","Access must be read or edit");
        if(data.empty()){
#ifdef _WIN32
            const char *base=std::getenv("LOCALAPPDATA");
#else
            const char *base=std::getenv("XDG_DATA_HOME");
#endif
            if(!base)throw cymcp::Error("InvalidArguments","Set --data-dir to a persistent private profile");
            data=pathUtf8(utf8Path(base)/"CyCodeReader"/"automation"/"cli");
        }
        const auto settings=config.empty()?Json::object():readJson(utf8Path(config));
        std::unique_ptr<Core> core;std::unique_ptr<Automation> host;std::shared_ptr<cymcp::Registry> registry;
        Json endpoint;
        if(connect.empty()){
            auto options=AutomationOptions::fromJson(settings);options.namedCopies=native.empty();
            if(native.empty()){core=std::make_unique<Core>(utf8Path(plugins));for(const auto &file:files)core->add(Document::open(utf8Path(file)));}
            host=std::make_unique<Automation>(utf8Path(data),[&](const auto &m,const auto &p,const auto &c){return native.empty()?dispatchTextSession(*core,m,p,c):legacyEndpointRequest(utf8Path(native),m,p,c);},false,options);
            if(settings.contains("remoteServers"))host->connectRemotes(settings.at("remoteServers"));registry=host->view(access);
        }else{
            endpoint=readJson(utf8Path(connect));const auto advertised=remoteRequest(endpoint,access,{{"method","describe"}});
            registry=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
            for(const auto &spec:advertised.at("operations")){
                cymcp::Operation op;op.name=spec.at("name");op.description=spec.at("description");
                op.input_schema=spec.at("inputSchema");op.output_schema=spec.value("outputSchema",Json{{"type","object"}});
                const auto name=op.name;op.handler=[endpoint,access,name](const Json &p,const cymcp::CallContext &c){
                    return remoteRequest(endpoint,access,{{"method","invoke"},{"name",name},{"arguments",p}},c.cancellation);};registry->add(std::move(op));
            }
        }
        const auto dispatch=[&](const Json &message){cymcp::CallContext c;c.request_id=cymcp::random_token(16);return cymcp::dispatch_app(*registry,message,c);};
        if(command=="list"||command=="describe"){
            std::cout<<(positional.size()>1?registry->describe(positional[1]):registry->describe()).dump()<<'\n';return 0;
        }
        if(command=="invoke"){
            if(positional.size()<2)throw cymcp::Error("InvalidArguments","Operation name required");
            auto name=positional[1];if(name.starts_with("op_"))name.erase(0,3);
            const auto parameters=!jsonFile.empty()?readJson(utf8Path(jsonFile)):cymcp::parse_json(jsonInput.empty()?"{}":jsonInput,20*1024*1024);
            std::cout<<dispatch({{"method","invoke"},{"name",name},{"arguments",parameters}}).dump()<<'\n';return 0;
        }
        if(command=="jsonl"){
            std::string line;while(std::getline(std::cin,line)){
                try{std::cout<<Json{{"result",dispatch(cymcp::parse_json(line,20*1024*1024))}}.dump()<<'\n';}
                catch(const cymcp::Error &e){std::cout<<Json{{"error",{{"code",e.code},{"message",cymcp::redact_error_message(e.what())}}}}.dump()<<'\n';}
                std::cout.flush();
            }return 0;
        }
        if(command=="mcp"){
            std::shared_ptr<const cymcp::Registry> tools=registry;std::unique_ptr<cymcp::ToolGateway> gateway;
            if(host)tools=host->tools(access,!full);
            else if(!full){gateway=std::make_unique<cymcp::ToolGateway>(registry);tools=gateway->tools();}
            auto server=host?host->mcp(tools):std::make_unique<cymcp::McpServer>(*tools,"CyCodeReader proxy");
            server->serve_stdio(std::cin,std::cout);return 0;
        }
        if(command=="serve"){
            if(!host)throw cymcp::Error("InvalidArguments","serve creates a headless instance; use mcp to proxy an existing one");
            host->start(descriptor.empty()?utf8Path(data)/"connection.json":utf8Path(descriptor));
            std::cerr<<"Reader service ready; descriptor: "<<(descriptor.empty()?pathUtf8(utf8Path(data)/"connection.json"):descriptor)<<'\n';
            if(legacy&&!native.empty()){while(std::filesystem::exists(utf8Path(native)))std::this_thread::sleep_for(std::chrono::milliseconds(100));}
            else {std::string line;std::getline(std::cin,line);}
            host->stop();return 0;
        }
        throw cymcp::Error("InvalidArguments","Unknown CLI command; use --help");
    }catch(const cymcp::Error &e){
        std::cerr<<Json{{"error",{{"code",e.code},{"message",cymcp::redact_error_message(e.what())}}}}.dump()<<'\n';
        return e.code=="AccessDenied"||e.code=="AuthenticationRequired"?3:e.code=="InvalidArguments"||e.code=="InvalidParameters"?2:1;
    }catch(const std::exception &e){std::cerr<<Json{{"error",{{"code","ReaderError"},{"message",cymcp::redact_error_message(e.what())}}}}.dump()<<'\n';return 1;}
}
}
