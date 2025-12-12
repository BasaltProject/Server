#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::player::playerPersistence {

/// Translates between the live player and the row that outlives them.
///
/// The mapping lives here, in the server, rather than in `server-storage`: the storage layer owns
/// how a player is written down, and knowing what a `PlayerSession` is would make it depend on the
/// server it is meant to sit underneath.

/// The row this session would be saved as. `nowEpochSeconds` is handed in so a caller decides what
/// "now" is rather than this reading a clock.
[[nodiscard]] storage::StoredPlayer toStored(const PlayerSession& session, std::string_view world,
                                             std::int64_t firstSeenEpochSeconds, std::int64_t playTimeTicks,
                                             std::int64_t nowEpochSeconds);

/// Puts a stored row back onto a freshly created session, in place.
void restore(PlayerSession& session, const storage::StoredPlayer& stored);

} // namespace basalt::server::runtime::player::playerPersistence
