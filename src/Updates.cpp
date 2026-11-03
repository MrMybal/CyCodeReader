#include "Updates.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <objbase.h>
#endif

namespace cycode::reader {
std::string updateRepository() { return CYCODE_READER_UPDATE_REPOSITORY; }
bool publicUpdatesEnabled() { return CYCODE_READER_PUBLIC_UPDATES!=0; }
namespace {
std::string apiRoot() { return "https://api.github.com/repos/"+updateRepository()+"/releases"; }
bool hexDigest(std::string_view value) { return value.size()==64 && std::all_of(value.begin(),value.end(),[](char c) { return (c>='0' && c<='9') || (c>='a' && c<='f'); }); }
#ifdef _WIN32
struct Internet {
    HINTERNET value=nullptr;
    explicit Internet(HINTERNET v):value(v) { if (!value) throw std::runtime_error("Cannot open update connection"); }
    ~Internet() { if (value) WinHttpCloseHandle(value); }
    Internet(const Internet &)=delete;
};
std::wstring wide(const std::string &text) {
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if (!count && !text.empty()) throw std::runtime_error("Invalid UTF-8 update text");
    std::wstring result(count,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),count);return result;
}
std::string narrow(const std::wstring &text) {
    const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if (!count && !text.empty()) throw std::runtime_error("Invalid Unicode update text");
    std::string result(count,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),count,nullptr,nullptr);return result;
}
void http(const std::string &initial,const std::string &accept,uint64_t maximum,const std::atomic<bool> &cancel,const std::function<void(const char *,size_t)> &consume) {
    const auto agent=wide("CyCodeReader-updates/"+applicationVersion());
    Internet session(WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    WinHttpSetTimeouts(session.value,5000,5000,5000,10000);
    std::string url=initial;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
    for (unsigned redirect=0;redirect<6;++redirect) {
        if (cancel) throw std::runtime_error("Update cancelled");
        const auto address=wide(url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
        parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=parts.dwPasswordLength=static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(address.c_str(),static_cast<DWORD>(address.size()),0,&parts) || parts.nScheme!=INTERNET_SCHEME_HTTPS || parts.nPort!=443 || parts.dwUserNameLength || parts.dwPasswordLength) throw std::runtime_error("Untrusted update URL");
        const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
        if (host!=L"api.github.com" && host!=L"release-assets.githubusercontent.com" && host!=L"objects.githubusercontent.com" && host!=L"github.com") throw std::runtime_error("Untrusted update host");
        if (!redirect && host!=L"api.github.com") throw std::runtime_error("Invalid GitHub API endpoint");
        const std::wstring resource=std::wstring(parts.lpszUrlPath,parts.dwUrlPathLength)+(parts.dwExtraInfoLength ? std::wstring(parts.lpszExtraInfo,parts.dwExtraInfoLength) : L"");
        Internet connection(WinHttpConnect(session.value,host.c_str(),443,0));
        Internet request(WinHttpOpenRequest(connection.value,L"GET",resource.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
        DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy))) throw std::runtime_error("Cannot secure update redirects");
        DWORD disabled=WINHTTP_DISABLE_AUTHENTICATION|WINHTTP_DISABLE_COOKIES;
        if (!WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))) throw std::runtime_error("Cannot disable update authentication");
        std::wstring headers=L"Accept: "+wide(accept)+L"\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        // Public releases only: no token lookup, credential storage or auth header.
        if (!WinHttpSendRequest(request.value,headers.c_str(),static_cast<DWORD>(headers.size()),WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request.value,nullptr)) throw std::runtime_error("GitHub update connection failed");
        DWORD status=0,length=sizeof(status);
        if (!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX)) throw std::runtime_error("Invalid GitHub update response");
        if (status==301 || status==302 || status==303 || status==307 || status==308) {
            DWORD bytes=0;WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&bytes,WINHTTP_NO_HEADER_INDEX);
            if (!bytes || bytes>32768) throw std::runtime_error("Invalid update redirect");
            std::wstring location(bytes/sizeof(wchar_t),0);
            if (!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&bytes,WINHTTP_NO_HEADER_INDEX)) throw std::runtime_error("Invalid update redirect");
            location.resize(wcslen(location.c_str()));url=narrow(location);continue;
        }
        if (status==404) throw std::runtime_error("No public release is available yet. Updates will work when the repository and its releases are public.");
        if (status==401 || status==403) throw std::runtime_error("Public GitHub access is unavailable or rate limited. Retry later.");
        if (status!=200) throw std::runtime_error("GitHub update request failed (HTTP "+std::to_string(status)+")");
        uint64_t total=0;std::array<char,65536> buffer{};
        while (true) {
            if (cancel) throw std::runtime_error("Update cancelled");
            if (std::chrono::steady_clock::now()>deadline) throw std::runtime_error("Update download timed out");
            DWORD count=0;if (!WinHttpReadData(request.value,buffer.data(),buffer.size(),&count)) throw std::runtime_error("Update download interrupted");
            if (!count) return;
            total+=count;if (total>maximum) throw std::runtime_error("Update exceeds its size limit");consume(buffer.data(),count);
        }
    }
    throw std::runtime_error("Too many update redirects");
}
#endif
}
ReaderRelease parseRelease(const nlohmann::json &json,Distribution mode) {
    if (mode==Distribution::development) throw std::runtime_error("Install a portable or installed distribution before updating");
    if (json.at("draft").get<bool>() || json.at("prerelease").get<bool>()) throw std::runtime_error("Only stable published releases are accepted");
    ReaderRelease result;result.version=json.at("tag_name").get<std::string>();releaseVersion(result.version);
    if (json.contains("body") && json.at("body").is_string()) result.notes=json.at("body").get<std::string>();
    if (result.notes.size()>32768) result.notes.resize(32768);
    const auto name="CyCodeReader-"+result.version.substr(result.version.starts_with('v') ? 1 : 0)+"-windows-x64-"+(mode==Distribution::portable ? "portable.zip" : "setup.exe");
    unsigned matches=0;
    for (const auto &asset : json.at("assets")) if (asset.value("name","")==name) {
        ++matches;
        result.asset.name=name;result.asset.apiUrl=asset.at("url");result.asset.size=asset.at("size").get<uint64_t>();
        const auto digest=asset.at("digest").get<std::string>();
        if (!digest.starts_with("sha256:") || !hexDigest(digest.substr(7))) throw std::runtime_error("Release asset has no valid GitHub SHA-256 digest");
        result.asset.sha256=digest.substr(7);
        const auto prefix=apiRoot()+"/assets/";
        const auto id=result.asset.apiUrl.substr(std::min(prefix.size(),result.asset.apiUrl.size()));
        if (!result.asset.apiUrl.starts_with(prefix) || id.empty() || !std::all_of(id.begin(),id.end(),[](char c) { return c>='0' && c<='9'; }) || !result.asset.size || result.asset.size>maximumUpdateSize || asset.value("state","")!="uploaded") throw std::runtime_error("Invalid release asset");
    }
    if (matches!=1) throw std::runtime_error("This release does not contain exactly one compatible Windows x64 package");
    return result;
}
std::optional<ReaderRelease> checkForUpdate(Distribution mode,const std::atomic<bool> &cancel) {
#ifdef _WIN32
    if (!publicUpdatesEnabled()) throw std::runtime_error("Public updates are not enabled in this preparation build");
    if (mode==Distribution::development) throw std::runtime_error("Install a portable or installed distribution before updating");
    std::string body;
    http(apiRoot()+"/latest","application/vnd.github+json",4*1024*1024,cancel,[&](const char *data,size_t size) { body.append(data,size); });
    const auto json=nlohmann::json::parse(body);
    if (!newerRelease(json.at("tag_name").get<std::string>(),applicationVersion())) return {};
    return parseRelease(json,mode);
#else
    (void)mode;(void)cancel;throw std::runtime_error("Application updates currently require Windows x64");
#endif
}
PreparedUpdate downloadUpdate(const ReaderRelease &release,const std::filesystem::path &installation,const std::atomic<bool> &cancel,std::atomic<uint64_t> &progress) {
#ifdef _WIN32
    GUID guid{};if (FAILED(CoCreateGuid(&guid))) throw std::runtime_error("Cannot create update directory");
    wchar_t id[40]{};StringFromGUID2(guid,id,40);
    const auto directory=readerDataDirectory(installation)/"updates"/std::filesystem::path(id);
    std::filesystem::create_directories(directory);
    PreparedUpdate result;result.installation=installation;result.mode=distribution(installation);result.sha256=release.asset.sha256;
    result.package=directory/release.asset.name;result.helper=directory/"CyCodeReaderUpdater.exe";
    const auto temporary=directory/"download.partial";
    try {
        std::ofstream out(temporary,std::ios::binary);if (!out) throw std::runtime_error("Cannot create update download");
        progress=0;
        http(release.asset.apiUrl,"application/octet-stream",release.asset.size,cancel,[&](const char *data,size_t size) { out.write(data,size);if (!out) throw std::runtime_error("Cannot write update download");progress+=size; });
        out.close();if (!out || progress!=release.asset.size || sha256File(temporary)!=release.asset.sha256) throw std::runtime_error("Update SHA-256 or size verification failed");
        std::filesystem::rename(temporary,result.package);
        std::filesystem::copy_file(installation/"CyCodeReaderUpdater.exe",result.helper);
        return result;
    } catch (...) {
        // The GUID directory was created by this operation and holds no user files.
        std::error_code ec;std::filesystem::remove_all(directory,ec);throw;
    }
#else
    (void)release;(void)installation;(void)cancel;(void)progress;throw std::runtime_error("Application updates currently require Windows x64");
#endif
}
void launchUpdate(const PreparedUpdate &update) {
#ifdef _WIN32
    if (distribution(update.installation)!=update.mode || update.mode==Distribution::development) throw std::runtime_error("Distribution changed before update");
    auto quote=[](const std::filesystem::path &p) { const auto value=p.wstring();if (value.find(L'"')!=std::wstring::npos) throw std::runtime_error("Invalid update path");return L"\""+value+L"\""; };
    std::wstring command=quote(update.helper)+L" --package "+quote(update.package)+L" --installation "+quote(update.installation)+L" --sha256 "+wide(update.sha256)+L" --parent "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if (!CreateProcessW(update.helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,update.helper.parent_path().c_str(),&startup,&process)) throw std::runtime_error("Cannot start the update helper");
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
#else
    (void)update;throw std::runtime_error("Application updates currently require Windows x64");
#endif
}
} // namespace cycode::reader
