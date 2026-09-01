#pragma once

#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>
#include <basalt/wire/auth/forwarding/VelocityForwarding.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::net {

/// One connection's side of proxy forwarding, kept beside the session: it all happens before there
/// is a player.
class ProxyLogin {
public:
    static constexpr std::int32_t kNoQuery = -1;

    /// Deliberately says nothing about which check failed.
    static constexpr std::string_view kRefusedMessage =
        "<red>This server only accepts players coming through its proxy.</red>\n"
        "<gray>If that is you, its forwarding is not set up on both ends.";

    ProxyLogin(wire::auth::forwarding::ProxyForwarding forwarding, wire::common::Logger logger)
        : forwarding_(std::move(forwarding)), logger_(std::move(logger)) {}

    /// Who the proxy says is connecting, once its data has been read and checked.
    [[nodiscard]] const std::optional<wire::auth::forwarding::ForwardedLogin>& login() const noexcept { return login_; }

    /// The query this connection is waiting on, or `kNoQuery` when it is not waiting on one.
    [[nodiscard]] std::int32_t transactionId() const noexcept { return transactionId_; }

    [[nodiscard]] bool enabled() const noexcept { return forwarding_.enabled(); }

    /// Whether the data rides on the handshake host, which is what the BungeeCord family does.
    [[nodiscard]] bool readsHandshake() const noexcept { return isLegacyForwarding(forwarding_.mode); }

    [[nodiscard]] bool asksTheProxy() const noexcept {
        return forwarding_.mode == wire::auth::forwarding::ForwardingMode::Velocity;
    }

    [[nodiscard]] std::string_view channel() const noexcept { return wire::auth::forwarding::velocity::kChannel; }

    /// Reads the handshake host, but the result is only acted on at login, where a refusal can
    /// still be a screen.
    [[nodiscard]] wire::auth::forwarding::ForwardingResult readHandshake(std::string_view host) const;

    /// Starts the Velocity query, returning the transaction id to send it under.
    std::int32_t beginQuery();

    /// What goes in the query body: the newest forwarding version this server can read.
    [[nodiscard]] std::vector<std::uint8_t> query() const;

    [[nodiscard]] wire::auth::forwarding::ForwardingResult
    readAnswer(std::int32_t answeredId, const std::optional<std::vector<std::uint8_t>>& payload);

    /// Returns the message to disconnect with, or nothing to carry on. The reason goes to the log,
    /// never to the screen.
    std::optional<std::string> accept(const wire::auth::forwarding::ForwardingResult& result,
                                      const std::optional<std::string>& name,
                                      const std::optional<std::string>& address);

private:
    wire::auth::forwarding::ProxyForwarding forwarding_;
    wire::common::Logger logger_;
    std::optional<wire::auth::forwarding::ForwardedLogin> login_;
    std::int32_t transactionId_ = kNoQuery;
};

} // namespace basalt::server::runtime::net
