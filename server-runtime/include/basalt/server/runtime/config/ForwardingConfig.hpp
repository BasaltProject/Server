#pragma once

#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

namespace forwardingConfig {

[[nodiscard]] std::expected<wire::auth::forwarding::ProxyForwarding, std::string>
fromToml(const wire::common::toml::TomlTable& root, std::string_view path);

void announce(const wire::auth::forwarding::ProxyForwarding& forwarding, bool onlineMode, std::int32_t port,
              const wire::common::Logger& logger);

} // namespace forwardingConfig

} // namespace basalt::server::runtime::config
