#pragma once

#include <basalt/server/remote/RemoteSettings.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config::remoteConfig {

/// Reads [network.rcon] and [network.api]; the API token is state and is filled in by the caller.
[[nodiscard]] std::expected<remote::RemoteSettings, std::string> fromToml(const wire::common::toml::TomlTable& root,
                                                                          std::string_view path);

} // namespace basalt::server::runtime::config::remoteConfig
