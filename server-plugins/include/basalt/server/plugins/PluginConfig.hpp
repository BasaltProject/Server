#pragma once

#include <basalt/server/plugins/PluginPackage.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <expected>
#include <functional>
#include <map>
#include <string>

namespace basalt::server::plugins::pluginConfig {
inline constexpr std::string_view kRootFile = "config.toml";

[[nodiscard]] std::expected<wire::common::toml::TomlTable, PluginLoadError>
load(const std::map<std::string, std::string, std::less<>>& files);
} // namespace basalt::server::plugins::pluginConfig
