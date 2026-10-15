#include <basalt/server/runtime/net/ProxyLogin.hpp>
#include <basalt/wire/auth/forwarding/LegacyForwarding.hpp>

#include <cstdint>
#include <random>
#include <utility>

namespace basalt::server::runtime::net {
namespace {

using wire::auth::forwarding::ForwardedLogin;
using wire::auth::forwarding::ForwardingResult;

// The id only has to be unpredictable enough that a second query cannot be answered with the first's reply.
[[nodiscard]] std::int32_t randomTransactionId() {
    static thread_local std::mt19937 generator{std::random_device{}()};
    return static_cast<std::int32_t>(generator());
}

} // namespace

ForwardingResult ProxyLogin::readHandshake(std::string_view host) const {
    return wire::auth::forwarding::legacy::decode(host, forwarding_);
}

std::int32_t ProxyLogin::beginQuery() {
    // Never the value meaning not waiting, so `readAnswer` cannot turn away a real answer once in four billion queries.
    std::int32_t id = randomTransactionId();
    while (id == kNoQuery) {
        id = randomTransactionId();
    }
    transactionId_ = id;
    return transactionId_;
}

std::vector<std::uint8_t> ProxyLogin::query() const { return wire::auth::forwarding::velocity::request(); }

ForwardingResult ProxyLogin::readAnswer(std::int32_t answeredId,
                                        const std::optional<std::vector<std::uint8_t>>& payload) {
    if (transactionId_ == kNoQuery) {
        // Nothing was asked, so nothing can be answered: otherwise the not-waiting id answers a query never made.
        return ForwardingResult{std::string{"the connection answered a login query nobody asked"}};
    }
    if (answeredId != transactionId_) {
        return ForwardingResult{std::string{"the login query answer names another query"}};
    }
    transactionId_ = kNoQuery;
    // A client that does not know the channel answers nothing: somebody came to this port directly, not via the proxy.
    if (!payload.has_value()) {
        return ForwardingResult{std::string{"the connection did not answer the forwarding query"}};
    }
    return wire::auth::forwarding::velocity::decode(*payload, forwarding_);
}

std::optional<std::string> ProxyLogin::accept(const ForwardingResult& result, const std::optional<std::string>& name,
                                              const std::optional<std::string>& address) {
    if (const ForwardedLogin* accepted = std::get_if<ForwardedLogin>(&result); accepted != nullptr) {
        login_ = *accepted;
        return std::nullopt;
    }
    logger_.warn([&result, &name, &address] {
        return "Refused " + name.value_or("a connection") + " from " + address.value_or("an unknown address") + ": " +
               std::get<std::string>(result) + ".";
    });
    return std::string{kRefusedMessage};
}

} // namespace basalt::server::runtime::net
