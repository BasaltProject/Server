#include <basalt/server/runtime/config/ForwardingConfig.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::config::forwardingConfig {

using wire::auth::forwarding::ForwardingMode;
using wire::auth::forwarding::forwardingModeName;
using wire::auth::forwarding::parseForwardingMode;
using wire::auth::forwarding::ProxyForwarding;
using wire::common::toml::TomlTable;

namespace {

[[nodiscard]] std::string everyModeName() {
    std::string names;
    for (const ForwardingMode mode : {ForwardingMode::None, ForwardingMode::BungeeCord, ForwardingMode::BungeeGuard,
                                      ForwardingMode::SafeNet, ForwardingMode::Velocity, ForwardingMode::Basalt}) {
        if (!names.empty()) {
            names += ", ";
        }
        names += forwardingModeName(mode);
    }
    return names;
}

[[nodiscard]] std::optional<wire::auth::forwarding::native::PublicKey> parsePublicKey(std::string_view hex) {
    if (hex.size() != 2 * wire::auth::forwarding::native::kPublicKeySize) {
        return std::nullopt;
    }
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    wire::auth::forwarding::native::PublicKey key{};
    for (std::size_t i = 0; i < key.size(); ++i) {
        const int high = nibble(hex[2 * i]);
        const int low = nibble(hex[2 * i + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        key[i] = static_cast<std::uint8_t>(high << 4 | low);
    }
    return key;
}

} // namespace

std::expected<ProxyForwarding, std::string> fromToml(const TomlTable& root, std::string_view path) {
    const TomlTable* network = root.table("network");
    const TomlTable* table = network == nullptr ? nullptr : network->table("forwarding");
    if (table == nullptr) {
        return ProxyForwarding{};
    }
    const std::string written{table->string("mode").value_or(forwardingModeName(ForwardingMode::None))};
    const std::optional<ForwardingMode> mode = parseForwardingMode(written);
    if (!mode.has_value()) {
        return std::unexpected(std::string{path} + ": forwarding mode must be one of " + everyModeName() + ", not \"" +
                               written + "\"");
    }
    if (*mode == ForwardingMode::None) {
        return ProxyForwarding{};
    }

    std::vector<std::string> tokens;
    for (std::string& token : table->stringList("tokens")) {
        if (!token.empty()) {
            tokens.push_back(std::move(token));
        }
    }
    ProxyForwarding forwarding{
        .mode = *mode,
        .secret = std::string{table->string("secret").value_or("")},
        .tokens = std::move(tokens),
        .propertyName = std::string{table->string("property-name").value_or(ProxyForwarding::kSafeNetProperty)},
    };
    for (const std::string& hex : table->stringList("trusted-keys")) {
        const auto key = parsePublicKey(hex);
        if (!key.has_value()) {
            return std::unexpected(std::string{path} + ": trusted-keys entries must be 64 hex characters, not \"" +
                                   hex + "\"");
        }
        forwarding.trustedKeys.push_back(*key);
    }
    forwarding.backendId = std::string{table->string("backend-id").value_or("")};
    forwarding.offerSessionEnvelope = table->boolean("session-envelope").value_or(false);
    // A mode without its secret refuses every login, and finding that out one player at a time is worse.
    if (const std::optional<std::string> broken = forwarding.misconfiguration(); broken.has_value()) {
        return std::unexpected(std::string{path} + ": " + *broken);
    }
    return forwarding;
}

void announce(const ProxyForwarding& forwarding, bool onlineMode, std::int32_t port,
              const wire::common::Logger& logger) {
    if (!forwarding.enabled()) {
        return;
    }
    logger.info([&forwarding] {
        return "Player data is forwarded by a " + std::string{forwardingModeName(forwarding.mode)} + " proxy.";
    });
    if (onlineMode) {
        logger.warn([] {
            return std::string{"online-mode is on and a proxy is forwarding: the proxy authenticates, so this "
                               "server will not. Set online-mode = false to stop this warning."};
        });
    }
    if (!forwarding.authenticated()) {
        logger.warn([&forwarding, port] {
            return std::string{forwardingModeName(forwarding.mode)} +
                   " forwarding carries no secret: anybody who can reach port " + std::to_string(port) +
                   " can claim to be any player. Firewall it, or use bungeeguard, safenet or velocity instead.";
        });
    }
}

} // namespace basalt::server::runtime::config::forwardingConfig
