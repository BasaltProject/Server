#pragma once

#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <optional>
#include <string>

namespace basalt::server::runtime::placeholder {

[[nodiscard]] const wire::common::placeholder::PlaceholderSlot<player::PlayerSession>& playerSlot();

[[nodiscard]] const wire::common::placeholder::PlaceholderSlot<config::PingRequest>& pingSlot();

[[nodiscard]] const wire::common::placeholder::PlaceholderSlot<player::PlayerSession>& otherPlayerSlot();

[[nodiscard]] wire::common::placeholder::PlaceholderValue notImplemented(std::string reason);

} // namespace basalt::server::runtime::placeholder
