#pragma once

#include <string>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Puts the middle-clicked item in the player's hand and answers whether anything changed.
///
/// parity: `Inventory.pickSlot`: hotbar items only move the selection, others are swapped in, and
/// only creative conjures one not carried.
[[nodiscard]] bool pickItemFor(PlayerSession& session, const std::string& id);

} // namespace basalt::server::runtime::player
