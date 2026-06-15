#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::plugins {

[[nodiscard]] server::plugins::PluginItem pluginItemOf(const storage::StoredItemStack& stack);

[[nodiscard]] storage::StoredItemStack storedItemOf(const server::plugins::PluginItem& item);

/// A stack as `{id:...,count:...,components:{...}}` text, which is how events hand an item to a plugin.
[[nodiscard]] std::string snbtOf(const storage::StoredItemStack& stack);

[[nodiscard]] std::optional<storage::StoredItemStack> stackFromSnbt(std::string_view text);

} // namespace basalt::server::runtime::plugins
