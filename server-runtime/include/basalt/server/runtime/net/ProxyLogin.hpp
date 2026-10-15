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

class ProxyLogin {
public:
    static constexpr std::int32_t kNoQuery = -1;

    static constexpr std::string_view kRefusedMessage =
        "<red>This server only accepts players coming through its proxy.</red>\n"
        "<gray>If that is you, its forwarding is not set up on both ends.";

    ProxyLogin(wire::auth::forwarding::ProxyForwarding forwarding, wire::common::Logger logger)
        : forwarding_(std::move(forwarding)), logger_(std::move(logger)) {}

    [[nodiscard]] const std::optional<wire::auth::forwarding::ForwardedLogin>& login() const noexcept { return login_; }

    [[nodiscard]] std::int32_t transactionId() const noexcept { return transactionId_; }

    [[nodiscard]] bool enabled() const noexcept { return forwarding_.enabled(); }

    [[nodiscard]] bool readsHandshake() const noexcept { return isLegacyForwarding(forwarding_.mode); }

    [[nodiscard]] bool asksTheProxy() const noexcept {
        return forwarding_.mode == wire::auth::forwarding::ForwardingMode::Velocity;
    }

    [[nodiscard]] std::string_view channel() const noexcept { return wire::auth::forwarding::velocity::kChannel; }

    [[nodiscard]] wire::auth::forwarding::ForwardingResult readHandshake(std::string_view host) const;

    std::int32_t beginQuery();

    [[nodiscard]] std::vector<std::uint8_t> query() const;

    [[nodiscard]] wire::auth::forwarding::ForwardingResult
    readAnswer(std::int32_t answeredId, const std::optional<std::vector<std::uint8_t>>& payload);

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
