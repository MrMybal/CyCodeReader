#pragma once
#include "Plugins.h"
#include <cyui/modules/Code.h>
#include <cyui/modules/Inspector.h>
#include <cyui/modules/Diagnostics.h>

namespace cycode::reader {
// Identity survives filtering, reordering and translated labels. Zero is reserved by CyUI.
cyui::ItemId uiId(const std::string &key);

struct MenuAction { std::string command;Json params;bool enabled=true; };
struct MenuModel {
    std::vector<cyui::FilterMenuItem> items;
    std::map<cyui::ItemId,MenuAction> actions;
    cyui::ItemId add(std::vector<cyui::FilterMenuItem> &,const std::string &label,const std::string &command,
                     Json params=Json::object(),const std::string &shortcut="",bool enabled=true,
                     bool checked=false,const std::string &keywords="");
};
// Cancel host drafts when an entire panel/section is omitted; options have no applied preview.
void cancelInspectorDrafts(cyui::InspectorState &);
struct DiagnosticFilterView {
    const std::vector<cyui::Diagnostic> &rows(const std::vector<cyui::Diagnostic> &,uint64_t revision,const cyui::TextFilter &);
  private:
    uint64_t revision_=~uint64_t(0);
    std::string query_,locale_;
    std::vector<cyui::Diagnostic> rows_;
};
struct PluginOptionsView {
    cyui::InspectorState state;
    std::uint64_t revision = 0;
    Json model;
    std::vector<cyui::Property> properties;
    std::map<cyui::ItemId, std::string> keys;
    void synchronize(const Json &plugin);
    std::string validate(cyui::ItemId, const cyui::PropertyValue &) const;
    // Draft updates/cancellations never touch options. Configure validates the entire commit.
    bool commit(Plugins &, const std::string &plugin, const std::vector<cyui::PropertyEdit> &);
};
} // namespace cycode::reader
