#include "PortableUpdate.h"
#include "Distribution.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace cycode::reader {
namespace {
using Json=nlohmann::json;
constexpr uint64_t limit=256ull*1024*1024;
struct Entry { std::string name;uint32_t offset=0,size=0,crc=0; };
uint32_t number(const std::string &bytes,size_t offset,size_t count) {
    if (offset>bytes.size() || count>bytes.size()-offset) throw std::runtime_error("Truncated portable archive");
    uint32_t result=0;for (size_t i=0;i<count;++i) result|=uint32_t(static_cast<unsigned char>(bytes[offset+i]))<<(8*i);return result;
}
uint32_t crc32(std::string_view bytes) {
    uint32_t crc=~0u;for (unsigned char byte : bytes) { crc^=byte;for (int bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u))); }return ~crc;
}
std::string caseKey(std::string text) { std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) { return char(std::tolower(c)); });return text; }
std::vector<Entry> entries(const std::string &bytes) {
    if (bytes.size()<22 || bytes.size()>limit) throw std::runtime_error("Invalid portable archive size");
    // The release packager writes no ZIP comments, ZIP64, compression or extras.
    const auto end=bytes.size()-22;
    if (number(bytes,end,4)!=0x06054b50 || number(bytes,end+4,2) || number(bytes,end+6,2) || number(bytes,end+20,2)) throw std::runtime_error("Unsupported portable archive format");
    const auto count=number(bytes,end+10,2);size_t at=number(bytes,end+16,4);
    if (!count || count>4096 || count!=number(bytes,end+8,2) || at+number(bytes,end+12,4)!=end) throw std::runtime_error("Invalid portable archive directory");
    std::vector<Entry> result;std::set<std::string> names;uint64_t total=0;
    for (uint32_t i=0;i<count;++i) {
        if (number(bytes,at,4)!=0x02014b50 || number(bytes,at+8,2)&~0x800u || number(bytes,at+10,2) || number(bytes,at+20,4)!=number(bytes,at+24,4) || number(bytes,at+34,2)) throw std::runtime_error("Unsupported portable archive entry");
        const auto length=number(bytes,at+28,2),extra=number(bytes,at+30,2),comment=number(bytes,at+32,2);
        const auto external=number(bytes,at+38,4);const auto unixType=(external>>16)&0170000;
        if ((unixType && unixType!=0100000) || (external&0x10) || at+46+length+extra+comment>end) throw std::runtime_error("Portable archive contains a special file");
        Entry entry{bytes.substr(at+46,length),number(bytes,at+42,4),number(bytes,at+24,4),number(bytes,at+16,4)};
        if (!packagePathAllowed(entry.name) || !names.insert(caseKey(entry.name)).second) throw std::runtime_error("Unsafe or duplicate portable archive path");
        const auto local=size_t(entry.offset);
        if (number(bytes,local,4)!=0x04034b50 || number(bytes,local+6,2)!=number(bytes,at+8,2) || number(bytes,local+8,2) || number(bytes,local+14,4)!=entry.crc || number(bytes,local+18,4)!=entry.size || number(bytes,local+22,4)!=entry.size) throw std::runtime_error("Invalid portable archive local header");
        const auto localName=number(bytes,local+26,2),localExtra=number(bytes,local+28,2);
        if (localName!=length || bytes.substr(local+30,localName)!=entry.name || local+30+localName+localExtra+entry.size>number(bytes,end+16,4)) throw std::runtime_error("Invalid portable archive data range");
        entry.offset=static_cast<uint32_t>(local+30+localName+localExtra);
        total+=entry.size;if (total>limit) throw std::runtime_error("Portable archive exceeds extraction limit");
        if (crc32(std::string_view(bytes).substr(entry.offset,entry.size))!=entry.crc) throw std::runtime_error("Corrupt portable archive entry");
        result.push_back(entry);at+=46+length+extra+comment;
    }
    if (at!=end) throw std::runtime_error("Invalid portable archive directory length");
    for (const auto &name : names) for (auto slash=name.find('/');slash!=std::string::npos;slash=name.find('/',slash+1)) if (names.contains(name.substr(0,slash))) throw std::runtime_error("Conflicting portable archive paths");
    return result;
}
void safeAncestors(const std::filesystem::path &root,const std::filesystem::path &relative) {
    auto path=root;
    for (const auto &part : relative) {
        path/=part;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(path))) throw std::runtime_error("Update target contains a symbolic link");
#ifdef _WIN32
        const auto attributes=GetFileAttributesW(path.c_str());
        if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) throw std::runtime_error("Update target contains a reparse point");
#endif
    }
}
std::set<std::string> manifestFiles(const Json &manifest) {
    releaseVersion(manifest.at("version").get<std::string>());
    std::set<std::string> files,keys;
    for (const auto &item : manifest.at("files")) {
        const auto name=item.get<std::string>();
        if (!packagePathAllowed(name) || !keys.insert(caseKey(name)).second) throw std::runtime_error("Invalid portable package manifest");
        files.insert(name);
    }
    for (const auto *required : {"CyCodeReader.exe","CyCodeReaderUpdater.exe","portable.flag","locales/ui.json","package-files.json"}) if (!files.contains(required)) throw std::runtime_error("Incomplete portable package manifest");
    return files;
}
}
void applyPortableUpdate(const std::filesystem::path &archive,const std::filesystem::path &installation) {
    if (distribution(installation)!=Distribution::portable) throw std::runtime_error("Portable update requires a portable installation");
    const auto root=std::filesystem::canonical(installation);
    safeAncestors(root.root_path(),root.relative_path());
    const auto size=std::filesystem::file_size(archive);if (size>limit) throw std::runtime_error("Portable archive exceeds size limit");
    std::ifstream input(archive,std::ios::binary);std::string bytes(static_cast<size_t>(size),0);input.read(bytes.data(),bytes.size());
    if (!input) throw std::runtime_error("Cannot read portable archive");
    const auto items=entries(bytes);
    const auto manifest=std::find_if(items.begin(),items.end(),[](const auto &entry) { return entry.name=="package-files.json"; });
    if (manifest==items.end()) throw std::runtime_error("Missing portable package manifest");
    const auto newManifest=Json::parse(std::string_view(bytes).substr(manifest->offset,manifest->size));
    const auto newFiles=manifestFiles(newManifest);
    std::set<std::string> actual;for (const auto &entry : items) actual.insert(entry.name);
    if (actual!=newFiles) throw std::runtime_error("Portable manifest does not match archive contents");
    auto affected=newFiles;
    if (std::filesystem::exists(root/"package-files.json")) {
        safeAncestors(root,"package-files.json");
        const auto previous=Json::parse(readTextFileBytes(root/"package-files.json"));
        if (!newerRelease(newManifest.at("version").get<std::string>(),previous.at("version").get<std::string>())) throw std::runtime_error("Portable update must be newer than the installed package");
        const auto oldFiles=manifestFiles(previous);affected.insert(oldFiles.begin(),oldFiles.end());
    }
    for (const auto &name : affected) {
        safeAncestors(root,utf8Path(name));
        if (std::filesystem::exists(root/utf8Path(name)) && !std::filesystem::is_regular_file(root/utf8Path(name))) throw std::runtime_error("Update destination is not a file");
    }
    const auto work=root/(".reader-update-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!std::filesystem::create_directory(work)) throw std::runtime_error("Cannot create update transaction");
    const auto fresh=work/"new",backup=work/"backup";
    std::vector<std::string> saved,installed;
    try {
        for (const auto &entry : items) {
            const auto destination=fresh/utf8Path(entry.name);std::filesystem::create_directories(destination.parent_path());
            std::ofstream out(destination,std::ios::binary);out.write(bytes.data()+entry.offset,entry.size);out.close();if (!out) throw std::runtime_error("Cannot extract portable update");
        }
        for (const auto &name : affected) if (std::filesystem::exists(root/utf8Path(name))) {
            const auto destination=backup/utf8Path(name);std::filesystem::create_directories(destination.parent_path());
            std::filesystem::rename(root/utf8Path(name),destination);saved.push_back(name);
        }
        for (const auto &name : newFiles) {
            const auto destination=root/utf8Path(name);std::filesystem::create_directories(destination.parent_path());
            std::filesystem::rename(fresh/utf8Path(name),destination);installed.push_back(name);
        }
    } catch (...) {
        const auto failure=std::current_exception();bool restored=true;
        for (auto it=installed.rbegin();it!=installed.rend();++it) { std::error_code ec;std::filesystem::remove(root/utf8Path(*it),ec);restored&=!ec; }
        for (auto it=saved.rbegin();it!=saved.rend();++it) { std::error_code ec;std::filesystem::rename(backup/utf8Path(*it),root/utf8Path(*it),ec);restored&=!ec; }
        if (!restored) throw std::runtime_error("Update rollback needs manual recovery from "+pathUtf8(backup));
        std::error_code ec;std::filesystem::remove_all(work,ec);std::rethrow_exception(failure);
    }
    // Only the unique transaction created above is removed; never user data.
    std::error_code ec;std::filesystem::remove_all(work,ec);
}
} // namespace cycode::reader
