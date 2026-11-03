#pragma once
#include "Document.h"
#include "Core.h"
#include "Bridge.h"
#include "NativeProcess.h"
#include "ModuleModels.h"
#include "Updates.h"
#include "cycode/Syntax.h"
#include <cyui/modules/CodeEditor.h>
#include <cyui/modules/Commands.h>
#include <cyui/modules/Diagnostics.h>
#include <cyui/modules/Diff.h>
#include <TextEditor.h>
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <vector>
#include <future>
namespace cycode::reader {
using Json=nlohmann::json;
struct Options {
    std::filesystem::path installation;
    std::vector<std::filesystem::path> files;
    std::string theme, language;
    bool automated=false;
    ImTextureID logo=0;
};
class Reader {
  public:
    Reader(Options options, SDL_Window *window);
    ~Reader();
    void frame();
    void open(const std::filesystem::path &path);
    void requestQuit();
    bool shouldQuit() const { return quit_; }
    std::optional<PreparedUpdate> pendingUpdate() const { return updateOnExit_; }
    void execute(const std::string &command, const Json &params=Json::object());
    void testAction(const Json &action);
    Json state() const;
  private:
    friend struct ReaderFilteringTests;
    friend struct ReaderUpdateTests;
    friend struct ReaderComparisonTests;
    friend struct ReaderExperienceTests;
    struct Tab {
        explicit Tab(std::shared_ptr<LiveDocument> entry) : live(std::move(entry)),document(live->document),editor(codeEditor.native()) {}
        std::shared_ptr<LiveDocument> live;
        Document &document;
        size_t id=0;
        cyui::CodeEditor codeEditor;
        TextEditor &editor; // Advanced native hooks use the same common editor instance.
        Syntax syntax;
        std::string syntaxPath;
        std::string language="auto";
        bool focus=true, diskChanged=false, detached=false;
        size_t version=0;
        uint64_t appliedRevision=0;
        std::string searchMarkerKey;
    };
    struct Dialog {
        std::mutex mutex;
        bool done=false, save=false, compare=false;
        size_t tab=0;
        std::string initial, error;
        std::vector<std::string> files;
    };
    struct PendingSave {
        size_t tab=0;
        std::filesystem::path path;
        std::optional<std::string> baseline;
    } overwrite_;
    std::vector<std::shared_ptr<Tab>> tabs_;
    std::shared_ptr<Tab> active() const;
    std::shared_ptr<Tab> find(size_t id) const;
    void newFile();
    void sync(const std::shared_ptr<Tab> &tab);
    void install(const std::shared_ptr<Tab> &tab);
    void setLanguage(const std::shared_ptr<Tab> &tab);
    void save(const std::shared_ptr<Tab> &tab, bool as=false);
    void saveAs(const std::shared_ptr<Tab> &tab, const std::filesystem::path &path);
    void commitSave(const PendingSave &pending);
    void requestClose(size_t id, bool reload=false);
    void resolveClose(const std::string &choice);
    void erase(size_t id);
    void showDialog(bool save, size_t tab=0,bool compare=false);
    void pollDialog();
    void saveNext();
    void menus();
    MenuModel menuModel(const std::string &) const;
    MenuModel providerMenuModel(size_t) const;
    void runMenuAction(const MenuModel &,cyui::ItemId);
    void registerFilterOwner(cyui::TextFilter &);
    bool routeFilterFind();
    void installTranslations();
    void shortcuts();
    void tools();
    void documents();
    void dialogs();
    void extensionPanels();
    void openFind(bool replace=false,bool all=false);
    void searchWindows();
    void rebuildSearch();
    void findMatch(bool previous=false);
    void replaceMatches(bool all);
    void updateSearchMarkers(const std::shared_ptr<Tab> &);
    void navigateSearch(const Json &,bool focus=true);
    void codeContext(size_t id,TextEditor::PopupData &,bool line);
    void historyAction(const std::string &,const Json &);
    void historyPanel();
    Json historyDiffParams(const std::string &,int index) const;
    void pollHistoryDiff();
    struct FindView {
        bool open=false,focus=false,replace=false,sensitive=false,words=false,wrap=true,extended=false,selection=false;
        size_t document=0,begin=0,end=0;
        uint64_t selectionRevision=0;
        std::string replacement,key,message;
        double changedAt=0;
    } find_;
    struct HistoryView {
        bool open=false,selection=false,focus=false,more=false;
        size_t document=0,provider=0,start=0,end=0;
        uint64_t revision=0;
        std::string path,message;
        Json commits=Json::array(),input=Json::object();
        int selected=-1,left=-1,right=-1;
        cyui::TextFilter filter;
        std::future<Json> pending;
        uint64_t generation=0;
        float listFraction=.28f;
        struct DiffPreview {
            bool ready=false;
            Json params=Json::object();
            std::string loadedKey,pendingKey,message,before,after,beforeLabel,afterLabel;
            cyui::DiffDocument document;
            cyui::DiffState state;
            std::future<Json> pending;
        } diff;
    } history_;
    Json historyComparison_=Json::object();
    std::string contextCommand_;
    Json contextParams_=Json::object();
    void checkUpdates();
    void updatePanel();
    void prepareFileProviders();
    void pollFileProviders(bool force=false);
    bool providerReady(size_t index,const std::string &command="") const;
    void compareAction(const std::string &,const Json &);
    void compareFile(size_t id,const std::filesystem::path &path);
    void showComparison(std::string before,std::string after,std::string left,std::string right,size_t leftId,size_t rightId,const std::string &kind);
    void startProviderComparison(size_t provider,const std::string &command,size_t document=0);
    void comparisonPanel();
    bool comparisonStale() const;
    void refreshComparison();
    struct FileProvider {
        std::string plugin,menu,detect,path;
        Json actions=Json::array(),context=Json::object(),history=Json::object();
        size_t document=0;
        double nextCheck=0;
        std::future<Json> pending;
    };
    struct Comparison {
        bool open=false,ready=false;
        std::string kind,before,after,leftLabel,rightLabel,path,command,message;
        size_t left=0,right=0,provider=0;
        uint64_t leftRevision=0,rightRevision=0;
        std::filesystem::file_time_type fileTime{};
        uintmax_t fileSize=0;
        bool diskChanged=false;
        double nextDiskCheck=0;
        cyui::DiffDocument document;
        cyui::DiffState state;
        Json providerParams=Json::object();
        std::future<Json> pending;
    } comparison_;
    std::vector<FileProvider> fileProviders_;
    size_t compareSelected_=0,compareLeft_=0,compareRight_=0;
    void applyAppearance();
    void startSdk();
    void nativeAction(const std::string &method,Json params=Json::object());
    void preparePluginLanguages();
    void remember(const std::filesystem::path &path);
    const char *tr(const char *key) const;
    std::string title(const std::shared_ptr<Tab> &tab) const;
    void error(const std::exception &exception);
    Options options_;
    Core core_;
    std::unique_ptr<Bridge> bridge_;
    std::future<cycode::ProcessResult> sdk_;
    std::atomic<bool> sdkStop_{false};
    std::filesystem::path sdkDescriptor_,python_;
    std::string sdkError_,pluginResult_;
    bool pluginsPanel_=false,mcpPanel_=false,optionsPanel_=false;
    bool searchPanel_=false,searchFocus_=false;
    cyui::CodeEditor searchPreview_;
    std::string searchPreviewKey_;
    cyui::ItemId selectedSearch_=0;
    std::future<std::optional<ReaderRelease>> releaseCheck_;
    std::future<PreparedUpdate> updateDownload_;
    std::optional<ReaderRelease> availableRelease_;
    std::optional<PreparedUpdate> preparedUpdate_,updateOnExit_;
    std::atomic<bool> updateCancel_{false};
    std::atomic<uint64_t> updateProgress_{0};
    bool updatePanel_=false,updateCheckOnStartup_=false,startupUpdateChecked_=false;
    std::string updateMessage_;
    std::string searchQuery_;
    std::string openEncoding_="auto";
    Json searchMatches_=Json::object();
    std::vector<cyui::Diagnostic> searchRows_;
    uint64_t searchRowsRevision_=0;
    DiagnosticFilterView searchFilteredRows_;
    cyui::TextFilter filesFilter_,optionsFilter_,pluginsFilter_,searchResultsFilter_;
    std::map<std::string,cyui::TextFilter> menuFilters_;
    std::map<ImGuiID,cyui::TextFilter *> filterOwners_;
    ImGuiID pluginOutputChild_=0;
    std::map<cyui::ItemId,Json> searchNavigation_;
    std::map<std::string,PluginOptionsView> pluginOptions_;
    cyui::CommandState commandState_;
    cyui::InspectorState optionsState_;
    Json optionsModel_;
    uint64_t optionsRevision_=0;
    cyui::LogBuffer serviceLogs_{2000,512*1024};
    cyui::LogState serviceLogState_;
    cyui::CodeEditor pluginOutput_;
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
    std::map<std::string,cyui::CodeLanguage> pluginSyntax_;
#endif
    std::map<std::string,std::unique_ptr<TextEditor::Language>> pluginLanguages_;
    std::map<std::string,std::string> pluginExtensions_;
    SDL_Window *window_;
    std::shared_ptr<Dialog> dialog_;
    Json translations_=Json::object(), trace_=Json::array();
    std::vector<std::string> recent_;
    std::vector<size_t> saveQueue_;
    size_t nextId_=1, active_=0, closeId_=0;
    bool reload_=false, quitting_=false, quit_=false, palette_=false, goto_=false;
    bool sidebar_=false, wrap_=false, minimap_=true, whitespace_=false;
    bool resetLayout_=true;
    float textScale_=1.15f;
    double pollAt_=0;
    int line_=1;
    ImGuiID dock_=0, editorDock_=0;
    std::string theme_="graphite", locale_="en", density_="compact", lastError_;
};
} // namespace cycode::reader
