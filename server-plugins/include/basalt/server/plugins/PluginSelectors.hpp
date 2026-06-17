#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginSelected {
    std::string uuid;
    std::string world;
    bool player = false;
};

/// The target selectors commands take (`@e[type=zombie,distance=..10]`), run for a player or for the console.
class PluginSelectors {
public:
    PluginSelectors() = default;
    PluginSelectors(const PluginSelectors&) = delete;
    PluginSelectors& operator=(const PluginSelectors&) = delete;
    PluginSelectors(PluginSelectors&&) = delete;
    PluginSelectors& operator=(PluginSelectors&&) = delete;
    virtual ~PluginSelectors() = default;

    [[nodiscard]] static PluginSelectors& unavailable();

    /// Who the selector picks as seen from the player with this uuid (empty: the console); an error text on failure.
    [[nodiscard]] virtual bool select(std::string_view selector, std::string_view asUuid,
                                      std::vector<PluginSelected>& into, std::string& error) {
        static_cast<void>(selector);
        static_cast<void>(asUuid);
        static_cast<void>(into);
        error = "selectors are not available";
        return false;
    }
};

inline PluginSelectors& PluginSelectors::unavailable() {
    static PluginSelectors none;
    return none;
}

} // namespace basalt::server::plugins
