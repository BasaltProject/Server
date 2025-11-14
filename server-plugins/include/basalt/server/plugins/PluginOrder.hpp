#pragma once

#include <basalt/server/plugins/PluginManifest.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

/// What the loader decided about one candidate.
struct PluginOrdering {
    PluginManifest manifest;
    /// Empty when it loads; otherwise why it does not, said the way its author can act on it.
    std::string refusal;

    [[nodiscard]] bool loads() const noexcept { return refusal.empty(); }
};

/// Works out what loads and in what order.
///
/// Deterministic: the same set of plugins gives the same order every time, with the ones that depend
/// on nothing going alphabetically. A server whose load order depends on the filesystem's mood is a
/// server whose bugs only happen on one machine.
namespace pluginOrder {

[[nodiscard]] std::vector<PluginOrdering> resolve(const std::vector<PluginManifest>& candidates);

/// Who would break if `id` went away, furthest first, so unloading can start at the leaves.
[[nodiscard]] std::vector<std::string> dependents(std::string_view id, const std::vector<PluginManifest>& loaded);

} // namespace pluginOrder

} // namespace basalt::server::plugins
