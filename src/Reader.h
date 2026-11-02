#pragma once
#include "Document.h"
#include "Core.h"
#include "Bridge.h"
#include "NativeProcess.h"
#include "cycode/Syntax.h"
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
};
class Reader {
  public:
    Reader(Options options, SDL_Window *window);
    ~Reader();
    void frame();
    void open(const std::filesystem::path &path);
    void requestQuit();
    bool shouldQuit() const { return quit_; }
    void execute(const std::string &command, const Json &params=Json::object());
    void testAction(const Json &action);
    Json state() const;
  private:
    struct Tab {
        explicit Tab(std::shared_ptr<LiveDocument> entry) : live(std::move(entry)),document(live->document) {}
        std::shared_ptr<LiveDocument> live;
        Document &document;
        size_t id=0;
        TextEditor editor;
        Syntax syntax;
        std::string syntaxPath;
        std::string language="auto";
        bool focus=true, diskChanged=false, detached=false;
        size_t version=0;
        uint64_t appliedRevision=0;
    };
    struct Dialog {
        std::mutex mutex;
        bool done=false, save=false;
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
    void showDialog(bool save, size_t tab=0);
    void pollDialog();
    void saveNext();
    void menus();
    void shortcuts();
    void tools();
    void documents();
    void dialogs();
    void extensionPanels();
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
    bool searchPanel_=false;
    std::string searchQuery_;
    std::string openEncoding_="auto";
    Json searchMatches_=Json::object();
    std::map<std::string,std::unique_ptr<TextEditor::Language>> pluginLanguages_;
    std::map<std::string,std::string> pluginExtensions_;
    SDL_Window *window_;
    std::shared_ptr<Dialog> dialog_;
    Json translations_=Json::object(), trace_=Json::array();
    std::vector<std::string> recent_;
    std::vector<size_t> saveQueue_;
    size_t nextId_=1, active_=0, closeId_=0;
    bool reload_=false, quitting_=false, quit_=false, palette_=false, goto_=false;
    bool sidebar_=true, wrap_=false, minimap_=false, whitespace_=false;
    bool resetLayout_=true;
    float textScale_=1.15f;
    double pollAt_=0;
    int line_=1;
    ImGuiID dock_=0, editorDock_=0;
    std::string theme_="graphite", locale_="en", lastError_, filter_, paletteQuery_;
};
} // namespace cycode::reader
