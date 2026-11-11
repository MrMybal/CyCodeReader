#pragma once
#include "cycode/PagedTextBackend.h"
#include <filesystem>
#include <memory>

namespace cycode {
// Graphical adapter only. LargeText owns bytes; immutable native snapshots own Undo.
// All public methods belong to the UI thread. Destruction joins the private worker.
class PagedTextEditor {
  public:
    using Match=PagedTextMatch;
    using Status=PagedTextStatus;
    // Injection mode never creates an autonomous LargeText, document or Undo stack.
    PagedTextEditor(cyui::ItemId,std::shared_ptr<PagedTextBackend>);
    void refresh(); // host calls after external native revision/read-only changes
    bool checkpoint(); // native durable publication; autonomous profile refuses
    PagedTextEditor(cyui::ItemId, const std::filesystem::path &source,
                    const std::filesystem::path &workingDirectory);
    ~PagedTextEditor();
    PagedTextEditor(const PagedTextEditor &)=delete;
    PagedTextEditor &operator=(const PagedTextEditor &)=delete;
    void poll();
    std::vector<cyui::PagedCodeCommand> draw(const char *, ImVec2 size={});
    const Status &status() const;
    cyui::PagedCodeEditor &view();
    void setLanguage(cyui::CodeLanguage);
    void setReadOnly(bool);
    bool command(cyui::PagedCodeCommand);
    bool paste();
    bool save(const std::filesystem::path &destination={});
    bool search(const std::string &, bool sensitive, bool words);
    bool find(bool previous, bool wrap=true);
    bool replace(const std::string &);
    void selectAll();
    void goTo(std::uint64_t line); // zero-based, UTF-16 column zero
    bool unsettled() const; // includes unacknowledged input and pending commits
    const std::vector<cyui::PagedCodeInput> &cancelledInput() const;
    const std::vector<cyui::PagedCodeReplacement> &cancelledEdits() const;
    // Host may copy these intents for manual recovery. Never automatically replay.
    std::string cancelledText() const;
    void acknowledgeCancelledInput();
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
