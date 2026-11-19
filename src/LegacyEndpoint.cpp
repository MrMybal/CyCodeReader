// Compatibility client for the original --serve TextBridge. No document model.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#endif
#include "Automation.h"
#include <algorithm>
#include <fstream>
namespace cycode::reader {
Json legacyEndpointRequest(const std::filesystem::path &path,const std::string &method,const Json &parameters,const cymcp::CallContext &context){
    context.cancellation.check();
    if(std::filesystem::file_size(path)>16384)throw cymcp::Error("InvalidDescriptor","Native descriptor exceeds its budget");
    std::ifstream file(path,std::ios::binary);const auto descriptor=Json::parse(file);
    if(descriptor.value("protocol","")!="cycodereader-native/1"||descriptor.value("host","")!="127.0.0.1")throw cymcp::Error("InvalidDescriptor","Expected a Reader native loopback descriptor");
    const auto port=descriptor.at("port").get<int>();if(port<1||port>65535)throw cymcp::Error("InvalidDescriptor","Invalid native port");
#ifdef _WIN32
    struct Winsock{Winsock(){WSADATA w{};if(WSAStartup(MAKEWORD(2,2),&w))throw cymcp::Error("TransportError","Winsock initialization failed");}~Winsock(){WSACleanup();}} winsock;
    struct Socket{SOCKET value=INVALID_SOCKET;~Socket(){if(value!=INVALID_SOCKET)closesocket(value);}} socket;
    constexpr auto invalid=INVALID_SOCKET;
#else
    struct Socket{int value=-1;~Socket(){if(value>=0)::close(value);}} socket;
    constexpr auto invalid=-1;
#endif
    socket.value=::socket(AF_INET,SOCK_STREAM,0);if(socket.value==invalid)throw cymcp::Error("TransportError","Cannot create native compatibility connection");
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(static_cast<unsigned short>(port));
    if(::connect(socket.value,reinterpret_cast<sockaddr*>(&address),sizeof(address)))throw cymcp::Error("TransportError","Native Reader is unavailable");
    auto bytes=Json{{"protocol","cycode-core/1"},{"id",cymcp::random_token(16)},{"token",descriptor.at("token")},{"method",method},{"params",parameters}}.dump()+"\n";
    if(bytes.size()>20*1024*1024)throw cymcp::Error("InvalidParameters","Native request exceeds its budget");
    for(size_t offset=0;offset<bytes.size();){context.cancellation.check();auto count=::send(socket.value,bytes.data()+offset,static_cast<int>(std::min(bytes.size()-offset,size_t(32768))),0);if(count<=0)throw cymcp::Error("TransportError","Native Reader disconnected while sending");offset+=count;}
    bytes.clear();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(200);
    for(;;){
        context.cancellation.check();if(std::chrono::steady_clock::now()>deadline)throw cymcp::Error("OutcomeUnknown","Native response timed out; inspect the document before any new action");
        fd_set ready;FD_ZERO(&ready);FD_SET(socket.value,&ready);timeval wait{0,100000};
        if(::select(static_cast<int>(socket.value+1),&ready,nullptr,nullptr,&wait)<=0)continue;
        char buffer[32768];const auto count=::recv(socket.value,buffer,sizeof(buffer),0);if(count<=0)throw cymcp::Error("OutcomeUnknown","Native Reader disconnected; inspect its state before any new action");
        bytes.append(buffer,count);if(bytes.size()>20*1024*1024)throw cymcp::Error("ResponseTooLarge","Native response exceeds its budget");
        if(bytes.find('\n')!=std::string::npos)break;
    }
    const auto reply=cymcp::parse_json(bytes,20*1024*1024);
    if(reply.contains("error"))throw cymcp::Error(reply["error"].value("code","NativeOperationFailed"),reply["error"].value("message","Native operation failed"));
    return reply.at("result");
}
}
