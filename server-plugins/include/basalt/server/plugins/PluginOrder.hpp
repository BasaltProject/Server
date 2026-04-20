#pragma once

#include <basalt/server/plugins/PluginManifest.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
struct PluginOrdering {
    PluginManifest manifest;
    /// Empty when it loads; otherwise why it does not, said the way its author can act on it.
    std::string refusal;

    [[nodiscard]] bool loads() const noexcept { return refusal.empty(); }
};

namespace pluginOrder {
[[nodiscard]] std::vector<PluginOrdering> resolve(const std::vector<PluginManifest>& candidates);

[[nodiscard]] std::vector<std::string> dependents(std::string_view id, const std::vector<PluginManifest>& loaded);
} // namespace pluginOrder
} // namespace basalt::server::plugins
