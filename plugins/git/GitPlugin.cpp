#include "PluginABI.h"
#include "NativeProcess.h"
#include "Encoding.h"
#include "cycode/TextFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {
using namespace cycode;using namespace cycode::reader;using Json=nlohmann::json;
constexpr auto timeout=std::chrono::seconds(4);
std::filesystem::path gitExecutable() {
    const auto value=std::getenv("PATH");std::istringstream paths(value ? value : "");std::string directory;
#ifdef _WIN32
    constexpr char separator=';';constexpr auto name="git.exe";
#else
    constexpr char separator=':';constexpr auto name="git";
#endif
    while (std::getline(paths,directory,separator)) {
        if (directory.size()>1 && directory.front()=='"' && directory.back()=='"') directory=directory.substr(1,directory.size()-2);
        const auto path=utf8Path(directory);std::error_code ec;
        if (path.is_absolute() && std::filesystem::is_regular_file(path/name,ec)) return std::filesystem::canonical(path/name);
    }
    throw std::runtime_error("Git is not installed or is not available in PATH");
}
ProcessResult git(const std::filesystem::path &executable,const std::filesystem::path &cwd,const std::vector<std::string> &args,size_t maximum=256*1024) {
    const std::vector<std::string> remove={"GIT_DIR","GIT_COMMON_DIR","GIT_WORK_TREE","GIT_INDEX_FILE","GIT_OBJECT_DIRECTORY","GIT_ALTERNATE_OBJECT_DIRECTORIES","GIT_NAMESPACE","GIT_SHALLOW_FILE","GIT_CONFIG","GIT_CONFIG_COUNT","GIT_CONFIG_PARAMETERS","GIT_CONFIG_SYSTEM","GIT_CONFIG_GLOBAL","GIT_GLOB_PATHSPECS","GIT_NOGLOB_PATHSPECS","GIT_ICASE_PATHSPECS","GIT_PREFIX","GIT_INTERNAL_SUPER_PREFIX"};
    std::vector<std::string> argv={pathUtf8(executable),"--no-pager","--no-optional-locks","--literal-pathspecs","-c","core.fsmonitor=false","-c","core.untrackedCache=false","-c","gc.auto=0","-c","maintenance.auto=false","-c","log.showSignature=false","-c","i18n.logOutputEncoding=UTF-8"};
    argv.insert(argv.end(),args.begin(),args.end());
    const auto result=runProcessFiltered(argv,cwd,timeout,maximum,nullptr,{{"GIT_TERMINAL_PROMPT","0"},{"GIT_OPTIONAL_LOCKS","0"},{"GIT_NO_LAZY_FETCH","1"},{"GIT_ALLOW_PROTOCOL",""},{"LC_ALL","C"}},remove);
    if (result.timedOut) throw std::runtime_error("Git comparison timed out");
    if (result.truncated) throw std::runtime_error("Git output exceeds the comparison limit");
    return result;
}
std::string line(std::string value) { if (value.ends_with('\n')) value.pop_back();if (value.ends_with('\r')) value.pop_back();return value; }
bool objectId(const std::string &id) { return (id.size()==40 || id.size()==64) && std::all_of(id.begin(),id.end(),[](unsigned char c) { return (c>='0' && c<='9') || (c>='a' && c<='f'); }); }
struct Blob { std::string id,mode;bool conflict=false; };
struct Context { std::filesystem::path executable,root;std::string relative,head;Blob index,committed;Json json; };
std::vector<std::string> records(const std::string &text) {
    std::vector<std::string> result;size_t at=0;
    while (at<text.size()) { const auto end=text.find('\0',at);if (end==std::string::npos) throw std::runtime_error("Malformed Git filename response");result.push_back(text.substr(at,end-at));at=end+1; }
    return result;
}
Blob entry(const std::string &record,const std::string &path,bool index) {
    const auto tab=record.find('\t');if (tab==std::string::npos || record.substr(tab+1)!=path) throw std::runtime_error("Unexpected Git filename response");
    std::istringstream fields(record.substr(0,tab));std::string mode,second,third;fields>>mode>>second>>third;
    const auto id=index ? second : third;
    if (!objectId(id) || (!index && second!="blob")) throw std::runtime_error("Git entry is not a regular text blob");
    if (mode!="100644" && mode!="100755") throw std::runtime_error("Git comparison supports regular text files only");
    return {id,mode,index && third!="0"};
}
Context context(const Json &input) {
    Context ctx;ctx.json={{"available",false},{"enabled",Json::object()}};
    const auto path=utf8Path(input.value("path",""));if (path.empty() || !path.is_absolute()) { ctx.json["message"]="Save this document before using Git comparisons";return ctx; }
    ctx.executable=gitExecutable();
    const auto parent=std::filesystem::weakly_canonical(path.parent_path());
    const auto discovered=git(ctx.executable,parent,{"rev-parse","--path-format=absolute","--show-toplevel"});
    if (discovered.exitCode) {
        const bool ordinary=discovered.output.find("not a git repository")!=std::string::npos;
        ctx.json["message"]=ordinary ? "This file is not in a Git working tree" : "Git could not inspect this file. Check repository access and Git ownership settings.";return ctx;
    }
    ctx.root=std::filesystem::canonical(utf8Path(line(discovered.output)));
    const auto full=parent/path.filename();const auto relative=full.lexically_relative(ctx.root);
    if (relative.empty() || relative.is_absolute()) throw std::runtime_error("File is outside the Git working tree");
    for (const auto &part : relative) if (part==".." || part==".git") throw std::runtime_error("File is outside the Git working tree");
    ctx.relative=pathUtf8(relative);
    const auto indexed=git(ctx.executable,ctx.root,{"ls-files","--stage","-z","--",ctx.relative});
    if (indexed.exitCode) throw std::runtime_error("Git could not read the index");
    for (const auto &record : records(indexed.output)) { const auto blob=entry(record,ctx.relative,true);if (blob.conflict) ctx.index.conflict=true;else { const bool conflict=ctx.index.conflict;ctx.index=blob;ctx.index.conflict=conflict; } }
    const auto head=git(ctx.executable,ctx.root,{"rev-parse","--verify","--quiet","HEAD^{commit}"});
    if (!head.exitCode) {
        ctx.head=line(head.output);if (!objectId(ctx.head)) throw std::runtime_error("Invalid Git HEAD object");
        const auto committed=git(ctx.executable,ctx.root,{"ls-tree","-z",ctx.head,"--",ctx.relative});
        if (committed.exitCode) throw std::runtime_error("Git could not inspect HEAD");
        const auto values=records(committed.output);if (values.size()>1) throw std::runtime_error("Ambiguous Git file entry");
        if (!values.empty()) ctx.committed=entry(values.front(),ctx.relative,false);
    }
    const auto branch=git(ctx.executable,ctx.root,{"symbolic-ref","--quiet","--short","HEAD"});
    const bool staged=ctx.index.id!=ctx.committed.id;
    ctx.json={{"available",true},{"root",pathUtf8(ctx.root)},{"relativePath",ctx.relative},{"branch",branch.exitCode ? "Detached HEAD" : line(branch.output)},
        {"tracked",!ctx.index.id.empty() || !ctx.committed.id.empty()},{"inIndex",!ctx.index.id.empty()},{"inHead",!ctx.committed.id.empty()},{"conflicted",ctx.index.conflict},{"staged",staged},{"detached",branch.exitCode!=0},
        {"enabled",{{"head",true},{"index",!ctx.index.conflict},{"staged",!ctx.index.conflict && (!ctx.index.id.empty() || !ctx.committed.id.empty())},{"history",!ctx.head.empty()},{"selectionHistory",!ctx.committed.id.empty()},{"revision",!ctx.head.empty()}}}};
    return ctx;
}
std::string text(const Context &ctx,const Blob &blob,const Json &input) {
    if (blob.id.empty()) return {};
    const auto size=git(ctx.executable,ctx.root,{"cat-file","-s",blob.id});
    if (size.exitCode) throw std::runtime_error("Git object is unavailable locally");
    const auto value=line(size.output);size_t used=0;const auto count=std::stoull(value,&used);
    if (used!=value.size() || count>maxTextFileBytes) throw std::runtime_error("Git text blob exceeds 4 MiB");
    const auto bytes=git(ctx.executable,ctx.root,{"cat-file","blob",blob.id},maxTextFileBytes+1024);
    if (bytes.exitCode || bytes.output.size()!=count) throw std::runtime_error("Git object could not be read locally");
    const auto requested=input.value("encoding","")=="windows-1252" && !bytes.output.starts_with("\xff\xfe") && !bytes.output.starts_with("\xfe\xff") && !bytes.output.starts_with("\xef\xbb\xbf") ? "windows-1252" : "auto";
    return normalizeEditorText(decodeText(bytes.output,requested).text);
}
std::string relativePath(const std::string &path) {
    const auto parsed=utf8Path(path);
    if (path.empty() || parsed.is_absolute() || path.find('\0')!=std::string::npos) throw std::runtime_error("Invalid historical Git path");
    for (const auto &part : parsed) if (part==".." || part==".git") throw std::runtime_error("Invalid historical Git path");
    return path;
}
std::string trim(std::string token) { while (!token.empty() && (token.front()=='\n' || token.front()=='\r')) token.erase(0,1);return token; }
constexpr auto historyFormat="--format=reader-commit%x00%H%x00%P%x00%an%x00%aI%x00%s%x00";
Json parseHistory(std::string output,std::string path,bool names) {
    if (!output.empty() && output.back()!='\0') output+='\0';
    const auto tokens=records(output);Json commits=Json::array();size_t at=0;
    while (at<tokens.size()) {
        auto token=trim(tokens[at++]);if (token.empty()) continue;
        if (token!="reader-commit" || at+5>tokens.size()) throw std::runtime_error("Malformed Git history response");
        const auto id=tokens[at++],parents=tokens[at++],author=tokens[at++],date=tokens[at++],subject=tokens[at++];
        if (!objectId(id) || author.size()>4096 || subject.size()>16384 || date.size()>128) throw std::runtime_error("Invalid Git history metadata");
        Json parentIds=Json::array();std::istringstream fields(parents);std::string parent;while (fields>>parent) { if (!objectId(parent)) throw std::runtime_error("Invalid Git history parent");parentIds.push_back(parent); }
        Json commit={{"id",id},{"parents",parentIds},{"author",author},{"date",date},{"subject",subject},{"path",path}};
        while (names && at<tokens.size() && trim(tokens[at])!="reader-commit") {
            const auto status=trim(tokens[at++]);if (status.empty()) continue;
            if (at>=tokens.size() || std::string("AMDRCTU").find(status.front())==std::string::npos) throw std::runtime_error("Invalid Git history status");
            const auto old=relativePath(tokens[at++]);
            if (status.front()=='R' || status.front()=='C') { if (at>=tokens.size()) throw std::runtime_error("Incomplete Git rename");const auto next=relativePath(tokens[at++]);if (next==path) { path=old;commit["previousPath"]=old; } }
        }
        if (!commit.contains("previousPath")) commit["previousPath"]=commit.at("path");
        commits.push_back(std::move(commit));
    }
    return commits;
}
Json fileHistory(const Context &ctx,const Json &cursor=Json::object(),size_t count=101) {
    const auto revision=cursor.value("revision",ctx.head),path=relativePath(cursor.value("path",ctx.relative));
    if (revision.empty()) return Json::array();if (!objectId(revision)) throw std::runtime_error("Invalid Git history cursor");
    const auto result=git(ctx.executable,ctx.root,{"log","--follow","--find-renames","--name-status","-z","--no-ext-diff","--no-textconv",historyFormat,"--max-count="+std::to_string(count),revision,"--",path},4*1024*1024);
    if (result.exitCode) throw std::runtime_error("Git could not read local file history");
    return parseHistory(result.output,path,true);
}
Blob revisionBlob(const Context &ctx,const std::string &revision,const std::string &path) {
    if (!objectId(revision)) throw std::runtime_error("Choose a valid Git revision");
    const auto result=git(ctx.executable,ctx.root,{"ls-tree","-z",revision,"--",relativePath(path)});
    if (result.exitCode) throw std::runtime_error("Historical Git revision is unavailable locally");
    const auto rows=records(result.output);if (rows.size()>1) throw std::runtime_error("Ambiguous historical Git file");
    return rows.empty() ? Blob{} : entry(rows.front(),path,false);
}
std::string pathAtRevision(const Context &ctx,const std::string &revision) {
    if (!objectId(revision)) throw std::runtime_error("Choose a valid Git revision");
    const auto result=git(ctx.executable,ctx.root,{"log","--follow","--find-renames","--format=","--name-status","-z","--no-ext-diff","--no-textconv",revision+".."+ctx.head,"--",ctx.relative},4*1024*1024);
    if (result.exitCode) throw std::runtime_error("Historical filename could not be resolved");
    const auto tokens=records(result.output);auto path=ctx.relative;
    for (size_t i=0;i<tokens.size();) {
        const auto status=trim(tokens[i++]);if (status.empty()) continue;
        if (i>=tokens.size() || std::string("AMDRCTU").find(status.front())==std::string::npos) throw std::runtime_error("Invalid Git rename response");
        const auto old=relativePath(tokens[i++]);
        if (status.front()=='R' || status.front()=='C') { if (i>=tokens.size()) throw std::runtime_error("Incomplete Git rename");const auto next=relativePath(tokens[i++]);if (next==path) path=old; }
    }
    return path;
}
Json history(const Context &ctx,const Json &input,bool selection) {
    Json rows;
    if (!selection) rows=fileHistory(ctx,input.value("cursor",Json::object()));
    else {
        if (ctx.committed.id.empty()) throw std::runtime_error("This selection has no committed history");
        const auto head=text(ctx,ctx.committed,input),buffer=normalizeEditorText(input.at("text"));
        const size_t start=input.at("startLine"),end=input.at("endLine");
        if (!start || end<start || end>100000) throw std::runtime_error("Invalid Git selection range");
        auto lineStart=[](const std::string &source,size_t line) { size_t at=0;for (size_t n=1;n<line;++n) { const auto next=source.find('\n',at);if (next==std::string::npos) throw std::runtime_error("Selection is outside the document");at=next+1; }return at; };
        const auto a=lineStart(buffer,start);const auto last=buffer.find('\n',lineStart(buffer,end));
        const auto selected=buffer.substr(a,(last==std::string::npos ? buffer.size() : last+1)-a);
        size_t mapped=start;
        if (head!=buffer) {
            const auto offset=head.find(selected);
            if (selected.empty() || offset==std::string::npos || (offset && head[offset-1]!='\n') || head.find(selected,offset+1)!=std::string::npos) throw std::runtime_error("Selected lines differ from HEAD or are ambiguous. Compare with HEAD before viewing line history.");
            mapped=1+std::count(head.begin(),head.begin()+offset,'\n');
        }
        const auto cursor=input.value("cursor",Json::object());const auto skip=cursor.value("offset",0u);
        if (skip>100000 || (cursor.contains("head") && cursor.at("head")!=ctx.head)) throw std::runtime_error("History changed. Refresh the history view.");
        const auto result=git(ctx.executable,ctx.root,{"log","--no-patch","--no-ext-diff","--no-textconv",historyFormat,"--max-count=101","--skip="+std::to_string(skip),"-L",std::to_string(mapped)+","+std::to_string(mapped+end-start)+":"+ctx.relative,ctx.head},4*1024*1024);
        if (result.exitCode) throw std::runtime_error("Git could not trace these lines in local history");
        rows=parseHistory(result.output,ctx.relative,false);
    }
    Json cursor=Json::object();const bool more=rows.size()>100;
    if (more) {
        if (selection) cursor={{"offset",input.value("cursor",Json::object()).value("offset",0u)+100},{"head",ctx.head}};
        else cursor={{"revision",rows.at(100).at("id")},{"path",rows.at(100).at("path")}};
        rows.erase(rows.begin()+100,rows.end());
    }
    return {{"history",{{"path",input.at("path")},{"commits",rows},{"more",more},{"cursor",cursor},{"selection",selection}}}};
}
Json historicalComparison(const Context &ctx,const Json &input) {
    const auto beforeId=input.at("beforeRevision").get<std::string>(),afterId=input.value("afterRevision","");
    const auto beforePath=relativePath(input.contains("beforePath") ? input.at("beforePath").get<std::string>() : beforeId.empty() ? ctx.relative : pathAtRevision(ctx,beforeId));
    auto before=beforeId.empty() ? std::string{} : text(ctx,revisionBlob(ctx,beforeId,beforePath),input);
    auto after=afterId.empty() ? input.at("text").get<std::string>() : text(ctx,revisionBlob(ctx,afterId,relativePath(input.contains("afterPath") ? input.at("afterPath").get<std::string>() : pathAtRevision(ctx,afterId))),input);
    validateEditorText(after);
    return {{"comparison",{{"before",before},{"after",after},{"beforeLabel",beforeId.empty() ? "Empty version" : beforeId.substr(0,12)},{"afterLabel",afterId.empty() ? "Current buffer" : afterId.substr(0,12)},{"path",input.at("path")}}}};
}
const char *execute(const char *command,const char *serialized) {
    static std::string reply;
    try {
        const auto input=Json::parse(serialized);const std::string action(command);
        if (action!="context" && action!="head" && action!="index" && action!="staged" && action!="history" && action!="selectionHistory" && action!="revision") throw std::runtime_error("Unknown Git comparison command");
        const auto ctx=context(input);
        if (action=="context") reply=ctx.json.dump();
        else {
            if (!ctx.json.value("available",false)) throw std::runtime_error(ctx.json.value("message","Git comparison unavailable"));
            if (!ctx.json.at("enabled").value(action,false)) throw std::runtime_error("Resolve the Git index conflict before comparing this version");
            if (action=="history" || action=="selectionHistory") { reply=history(ctx,input,action=="selectionHistory").dump();return reply.c_str(); }
            if (action=="revision") { reply=historicalComparison(ctx,input).dump();return reply.c_str(); }
            const auto before=text(ctx,action=="index" ? ctx.index : ctx.committed,input);
            const auto after=action=="staged" ? text(ctx,ctx.index,input) : input.at("text").get<std::string>();validateEditorText(after);
            reply=Json{{"comparison",{{"before",before},{"after",after},{"beforeLabel",action=="index" ? "Git index" : "HEAD"},{"afterLabel",action=="staged" ? "Git index" : "Current buffer"},{"path",input.at("path")}}},{"context",ctx.json}}.dump();
        }
    } catch (const std::exception &error) { reply=Json{{"error",error.what()}}.dump(); }
    return reply.c_str();
}
}
#ifdef _WIN32
#define READER_EXPORT __declspec(dllexport)
#else
#define READER_EXPORT __attribute__((visibility("default")))
#endif
extern "C" READER_EXPORT const CyCodeReaderPluginV1 *cycodereader_plugin_v1() {
    static const CyCodeReaderPluginV1 api{CYCODE_READER_PLUGIN_ABI,sizeof(CyCodeReaderPluginV1),execute};return &api;
}
