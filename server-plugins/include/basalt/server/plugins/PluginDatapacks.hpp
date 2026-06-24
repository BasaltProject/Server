#pragma once

#include <string>
#include <vector>

namespace basalt::server::plugins {

struct PluginDatapack {
    std::string id;
    /// JSON text components.
    std::string title;
    std::string description;
    bool required = false;
    bool enabled = false;
    /// "compatible", "too_old", "too_new" or "unknown".
    std::string compatibility = "compatible";
    /// "built_in", "feature" or "world".
    std::string source = "world";
    std::vector<std::string> features;
};

/// The data packs the server has found and which of them are on.
class PluginDatapacks {
public:
    PluginDatapacks() = default;
    PluginDatapacks(const PluginDatapacks&) = delete;
    PluginDatapacks& operator=(const PluginDatapacks&) = delete;
    PluginDatapacks(PluginDatapacks&&) = delete;
    PluginDatapacks& operator=(PluginDatapacks&&) = delete;
    virtual ~PluginDatapacks() = default;

    [[nodiscard]] static PluginDatapacks& unavailable();

    /// Looks for packs added to the world's folder since the last look.
    virtual void refresh() {}

    /// Finds new packs and reloads the data of the server, as `/reload` does; false when the reload failed.
    virtual bool reload() { return false; }

    [[nodiscard]] virtual std::vector<PluginDatapack> packs() { return {}; }
};

inline PluginDatapacks& PluginDatapacks::unavailable() {
    static PluginDatapacks none;
    return none;
}

} // namespace basalt::server::plugins
