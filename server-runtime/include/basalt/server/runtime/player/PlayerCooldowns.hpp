#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Whether that item is still cooling down. parity: `ItemCooldowns.isOnCooldown`, and one that has
/// run out is forgotten here rather than left to pile up.
[[nodiscard]] bool onCooldownFor(PlayerSession& session, const wire::common::Identifier& item, std::int64_t gameTime);

/// Starts the item's own cooldown and tells the client. Does nothing for an item that has none.
///
/// Shared because both doors throw the same pearl: what differs is only which packet says so.
void startCooldownFor(PlayerSession& session, const wire::common::Identifier& item, std::int64_t gameTime);

} // namespace basalt::server::runtime::player
