#pragma once

#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

/// `[network.forwarding]` in `config/server.toml`: which proxy, if any, speaks for the players.
namespace forwardingConfig {

/// Reads `[network.forwarding]`, or a disabled forwarding when nothing is configured.
///
/// A mode spelled wrong, or one without the secret it needs, is a failure rather than a quiet
/// fallback: either one refuses every login, and finding that out one player at a time is worse
/// than being told at startup.
[[nodiscard]] std::expected<wire::auth::forwarding::ProxyForwarding, std::string>
fromToml(const wire::common::toml::TomlTable& root, std::string_view path);

/// Every mode turns off Mojang authentication here, so what that costs is said out loud.
void announce(const wire::auth::forwarding::ProxyForwarding& forwarding, bool onlineMode, std::int32_t port,
              const wire::common::Logger& logger);

} // namespace forwardingConfig

} // namespace basalt::server::runtime::config
