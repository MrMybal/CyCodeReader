#include "Distribution.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <bcrypt.h>
#endif

namespace cycode::reader {
std::string applicationVersion() { return CYCODE_READER_VERSION; }
std::array<unsigned,3> releaseVersion(std::string_view text) {
    if (text.starts_with('v')) text.remove_prefix(1);
    std::array<unsigned,3> result{};
    for (size_t i=0;i<result.size();++i) {
        const auto dot=text.find('.');
        const auto part=text.substr(0,dot);
        if (part.empty() || part.size()>5 || (part.size()>1 && part.front()=='0')) throw std::runtime_error("Invalid stable release version");
        const auto parsed=std::from_chars(part.data(),part.data()+part.size(),result[i]);
        if (parsed.ec!=std::errc{} || parsed.ptr!=part.data()+part.size() || result[i]>65535) throw std::runtime_error("Invalid stable release version");
        if (i<2) { if (dot==std::string_view::npos) throw std::runtime_error("Invalid stable release version");text.remove_prefix(dot+1); }
        else if (dot!=std::string_view::npos) throw std::runtime_error("Invalid stable release version");
    }
    return result;
}
bool newerRelease(std::string_view candidate,std::string_view current) { return releaseVersion(candidate)>releaseVersion(current); }
Distribution distribution(const std::filesystem::path &installation) {
    if (std::filesystem::is_regular_file(installation/"portable.flag")) return Distribution::portable;
    if (std::filesystem::is_regular_file(installation/"installed.flag")) return Distribution::installed;
    return Distribution::development;
}
std::filesystem::path readerDataDirectory(const std::filesystem::path &installation) {
    if (distribution(installation)!=Distribution::installed) return installation/"data";
#ifdef _WIN32
    PWSTR local=nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&local))) throw std::runtime_error("Cannot locate local application data");
    const auto result=std::filesystem::path(local)/"CyCodeReader";CoTaskMemFree(local);return result;
#else
    throw std::runtime_error("Installed distributions currently require Windows");
#endif
}
bool packagePathAllowed(std::string_view path) {
    // This is deliberately narrower than ZIP's path grammar. User data, unknown
    // root files, NTFS streams, reserved device names and alternate markers stay out.
    if (path.empty() || path.size()>240 || path.front()=='/' || path.find_first_of("\\:\0",0,3)!=std::string_view::npos) return false;
    size_t start=0;
    while (start<path.size()) {
        const auto end=path.find('/',start);const auto part=path.substr(start,end==std::string_view::npos ? path.size()-start : end-start);
        if (part.empty() || part=="." || part==".." || part.back()=='.' || part.back()==' ') return false;
        for (unsigned char c : part) if (c<32 || c>=127 || c=='<' || c=='>' || c=='"' || c=='|' || c=='?' || c=='*') return false;
        std::string stem(part.substr(0,part.find('.')));std::transform(stem.begin(),stem.end(),stem.begin(),[](unsigned char c) { return char(std::toupper(c)); });
        if (stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" || (stem.size()==4 && (stem.starts_with("COM") || stem.starts_with("LPT")) && stem[3]>='0' && stem[3]<='9')) return false;
        if (end==std::string_view::npos) break;
        start=end+1;if (start==path.size()) return false;
    }
    if (path=="CyCodeReader.exe" || path=="CyCodeReaderUpdater.exe" || path=="portable.flag" || path=="package-files.json" || path=="LICENSE" || path=="COPYRIGHT" || path=="THIRD_PARTY_NOTICES.md") return true;
    if (path.find('/')==std::string_view::npos) return path.ends_with(".dll");
    return path.starts_with("Documentation/") || path.starts_with("help/") || path.starts_with("locales/") || path.starts_with("licenses/") || path.starts_with("plugins/") || path.starts_with("app/") ||
        path=="fonts/JetBrainsMono-Regular.ttf" || path=="icons/CyCodeReader.png" || path=="icons/CyCodeReader.ico" || path=="icons/CyCodeReader-document.png" || path=="icons/CyCodeReader-document.ico";
}
std::string sha256File(const std::filesystem::path &path) {
#ifdef _WIN32
    struct Hash {
        BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
        ~Hash() { if (hash) BCryptDestroyHash(hash);if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0); }
    } state;
    if (BCryptOpenAlgorithmProvider(&state.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0 || BCryptCreateHash(state.algorithm,&state.hash,nullptr,0,nullptr,0,0)<0) throw std::runtime_error("Cannot initialize SHA-256");
    std::ifstream stream(path,std::ios::binary);if (!stream) throw std::runtime_error("Cannot read update file");
    std::array<char,65536> buffer{};
    while (stream) { stream.read(buffer.data(),buffer.size());if (stream.gcount() && BCryptHashData(state.hash,reinterpret_cast<PUCHAR>(buffer.data()),static_cast<ULONG>(stream.gcount()),0)<0) throw std::runtime_error("Cannot hash update file"); }
    if (!stream.eof()) throw std::runtime_error("Cannot read update file");
    std::array<unsigned char,32> digest{};if (BCryptFinishHash(state.hash,digest.data(),digest.size(),0)<0) throw std::runtime_error("Cannot finish SHA-256");
    constexpr char hex[]="0123456789abcdef";std::string result;for (auto byte : digest) { result+=hex[byte>>4];result+=hex[byte&15]; }return result;
#else
    throw std::runtime_error("Native update hashing currently requires Windows");
#endif
}
InstanceLock::InstanceLock(const std::filesystem::path &directory,bool exclusive,bool installed) {
#ifdef _WIN32
    std::filesystem::create_directories(directory);
    handle_=CreateFileW((directory/"instance.lock").c_str(),exclusive ? GENERIC_READ|GENERIC_WRITE : GENERIC_READ,exclusive ? 0 : FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (handle_==INVALID_HANDLE_VALUE) { handle_=nullptr;throw std::runtime_error("Another Reader instance is running or an update is in progress"); }
    if (installed) {
        mutex_=CreateMutexW(nullptr,FALSE,L"CyCodeReader.Installed");
        if (!mutex_) { CloseHandle(handle_);handle_=nullptr;throw std::runtime_error("Cannot create installation lock"); }
    }
#else
    (void)directory;(void)exclusive;(void)installed;
#endif
}
InstanceLock::~InstanceLock() {
#ifdef _WIN32
    if (handle_) CloseHandle(handle_);if (mutex_) CloseHandle(mutex_);
#endif
}
} // namespace cycode::reader
