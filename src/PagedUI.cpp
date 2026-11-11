#include "Reader.h"
#include "cycode/Paths.h"
#include <algorithm>

namespace cycode::reader {
bool Reader::pagedCommand(const std::string &command,const Json &) {
    auto tab=active();if (!tab || !tab->paged) return false;
    using C=cyui::PagedCodeCommand;
    const std::pair<const char *,C> actions[]={{"undo",C::undo},{"redo",C::redo},{"copy",C::copy},{"cut",C::cut}};
    for (const auto &[name,action]:actions) if (command==name) { tab->paged->command(action);return true; }
    if (command=="paste") { tab->paged->paste();return true; }
    if (command=="selectAll") {
        // Resolve the final UTF-16 position outside the UI thread.
        tab->paged->selectAll();return true;
    }
    if (command=="format" || command=="plugin" || command.starts_with("transform."))
        throw std::runtime_error(tr("This command requires an ordinary text document."));
    return false;
}
void Reader::drawPaged(const std::shared_ptr<Tab> &tab) {
    cyui::textDim("%s",tr("Paged UTF-8: native storage, asynchronous pages, full-document saves."));
    if (cyui::ghostButton(tr("Paged text help"))) execute("help",{{"page","paged-text"}});
    if (!tab->paged->cancelledInput().empty() || !tab->paged->cancelledEdits().empty()) {
        cyui::callout(cyui::Tone::warning,tr("Pending input was cancelled. Recover it before closing or saving."));
        if (cyui::button(tr("Copy pending input"))) {
            const auto text=tab->paged->cancelledText();ImGui::SetClipboardText(text.c_str());
        }
        ImGui::SameLine();if (cyui::button(tr("Acknowledge pending input"))) tab->paged->acknowledgeCancelledInput();
    }
    using C=cyui::PagedCodeCommand;
    for (const auto action:tab->paged->draw(("##reader-paged-"+std::to_string(tab->id)).c_str())) {
        if (action==C::save) execute("save");
        else if (action==C::find) openFind();
        else if (action==C::findNext) findMatch();
        else if (action==C::findPrevious) findMatch(true);
    }
}
}
