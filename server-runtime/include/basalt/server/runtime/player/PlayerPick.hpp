#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <string>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Puts the middle-clicked stack in the player's hand and answers whether anything changed.
///
/// parity: `ServerGamePacketListenerImpl.tryPickItem`: the same item with the same components already
/// carried is selected in the hotbar or swapped into it, and only creative conjures one not carried.
[[nodiscard]] bool pickItemFor(PlayerSession& session, const storage::StoredItemStack& picked);

/// The same for a bare item, which is what every pick without block data is.
[[nodiscard]] bool pickItemFor(PlayerSession& session, const std::string& id);

} // namespace basalt::server::runtime::player
