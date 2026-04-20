#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <string>

namespace basalt::server::runtime::player {

class PlayerSession;

[[nodiscard]] bool pickItemFor(PlayerSession& session, const storage::StoredItemStack& picked);

[[nodiscard]] bool pickItemFor(PlayerSession& session, const std::string& id);

} // namespace basalt::server::runtime::player
