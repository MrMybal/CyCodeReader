#include "ModuleModels.h"
#include <limits>

namespace cycode::reader {
cyui::ItemId uiId(const std::string &key) {
    std::uint64_t value = 14695981039346656037ull;
    for (unsigned char byte : key) { value ^= byte; value *= 1099511628211ull; }
    return value ? value : 1;
}
cyui::ItemId MenuModel::add(std::vector<cyui::FilterMenuItem> &into,const std::string &label,
    const std::string &command,Json params,const std::string &shortcut,bool enabled,bool checked,const std::string &keywords) {
    if (params.is_null()) params=Json::object();
    const auto id=uiId("reader/menu/"+command+"/"+params.dump());
    if (!actions.emplace(id,MenuAction{command,std::move(params),enabled}).second) throw std::runtime_error("Duplicate menu action identity");
    into.push_back({id,label,shortcut,keywords,enabled,checked,{}});return id;
}
void cancelInspectorDrafts(cyui::InspectorState &state) {
    for (auto &[id,draft] : state.drafts) if (draft.active) {
        draft.value=draft.before;draft.active=false;draft.cancelled=true;draft.error.clear();
    }
}
const std::vector<cyui::Diagnostic> &DiagnosticFilterView::rows(const std::vector<cyui::Diagnostic> &source,
    uint64_t revision,const cyui::TextFilter &filter) {
    if (revision_==revision && query_==filter.query && locale_==cyui::locale()) return rows_;
    rows_.clear();
    for (const auto &row : source) if (filter.matches(row.source,row.source+" "+row.message+" "+std::to_string(row.line)+" "+std::to_string(row.column))) rows_.push_back(row);
    revision_=revision;query_=filter.query;locale_=cyui::locale();return rows_;
}
void PluginOptionsView::synchronize(const Json &plugin) {
    if (model == plugin) return;
    model = plugin; ++revision;
    properties.clear(); keys.clear();
    for (const auto &definition : plugin.at("optionDefinitions")) {
        const auto key = definition.at("id").get<std::string>();
        const auto &value = plugin.at("options").at(key);
        cyui::Property property;
        property.id = uiId("plugin/" + plugin.at("id").get<std::string>() + "/option/" + key);
        property.label = key;
        if (value.is_boolean()) property.value = value.get<bool>();
        else if (value.is_number_integer()) {
            property.value = value.get<std::int64_t>(); property.bounded = true;
            property.integerMinimum = std::numeric_limits<std::int32_t>::min();
            property.integerMaximum = std::numeric_limits<std::int32_t>::max();
        } else property.value = value.get<std::string>();
        if (!keys.emplace(property.id, key).second) throw std::runtime_error("Plugin option UI identity collision");
        properties.push_back(std::move(property));
    }
}
std::string PluginOptionsView::validate(cyui::ItemId id, const cyui::PropertyValue &value) const {
    const auto key = keys.find(id);
    if (key == keys.end()) return "Unknown plugin option";
    const auto property = std::find_if(properties.begin(), properties.end(),
                                      [id](const auto &p) { return p.id == id; });
    if (property == properties.end() || property->value.index() != value.index()) return "Invalid option type";
    if (const auto *text = std::get_if<std::string>(&value); text && text->size() > 4096)
        return "Plugin strings are limited to 4096 UTF-8 bytes";
    if (const auto *number = std::get_if<std::int64_t>(&value); number &&
        (*number < std::numeric_limits<std::int32_t>::min() || *number > std::numeric_limits<std::int32_t>::max()))
        return "Plugin integer option exceeds 32-bit range";
    return {};
}
bool PluginOptionsView::commit(Plugins &plugins, const std::string &plugin,
                               const std::vector<cyui::PropertyEdit> &edits) {
    Json patch = Json::object();
    for (const auto &proposal : edits) if (proposal.edit.committed()) {
        const auto error = validate(proposal.id, proposal.edit.value);
        if (!error.empty()) throw std::runtime_error(error);
        std::visit([&](const auto &value) { patch[keys.at(proposal.id)] = value; }, proposal.edit.value);
    }
    if (patch.empty()) return false;
    plugins.configure(plugin, patch);
    return true;
}
} // namespace cycode::reader
