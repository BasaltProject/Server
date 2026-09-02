#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>

#include <utility>

namespace basalt::server::runtime::placeholder {

const wire::common::placeholder::PlaceholderSlot<player::PlayerSession>& playerSlot() {
    static const wire::common::placeholder::PlaceholderSlot<player::PlayerSession> slot{"player"};
    return slot;
}

const wire::common::placeholder::PlaceholderSlot<config::PingRequest>& pingSlot() {
    static const wire::common::placeholder::PlaceholderSlot<config::PingRequest> slot{"ping"};
    return slot;
}

const wire::common::placeholder::PlaceholderSlot<player::PlayerSession>& otherPlayerSlot() {
    static const wire::common::placeholder::PlaceholderSlot<player::PlayerSession> slot{"other_player"};
    return slot;
}

wire::common::placeholder::PlaceholderValue notImplemented(std::string reason) {
    return wire::common::placeholder::PlaceholderValue::unavailable(std::move(reason));
}

} // namespace basalt::server::runtime::placeholder
