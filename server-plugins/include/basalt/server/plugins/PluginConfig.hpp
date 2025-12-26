#pragma once

#include <basalt/server/plugins/PluginPackage.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <expected>
#include <functional>
#include <map>
#include <string>

namespace basalt::server::plugins::pluginConfig {

/// A `config.toml` at the top is the exception: its keys go straight on `config`.
inline constexpr std::string_view kRootFile = "config.toml";

/// A plugin's configuration: whichever TOML files it has, wherever it put them.
///
/// Addressed by name and not by place, so `modules/chat.toml` is `config.chat` and moving it does
/// not change a line of Lua. Which is why two files cannot share a name.
[[nodiscard]] std::expected<wire::common::toml::TomlTable, PluginLoadError>
load(const std::map<std::string, std::string, std::less<>>& files);

} // namespace basalt::server::plugins::pluginConfig
