#pragma once
#include "Plugins.h"
#include <cyui/modules/Code.h>
#include <cyui/modules/Inspector.h>

namespace cycode::reader {
// Identity survives filtering, reordering and translated labels. Zero is reserved by CyUI.
cyui::ItemId uiId(const std::string &key);

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
