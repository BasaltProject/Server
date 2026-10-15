#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::player::playerPersistence {

[[nodiscard]] storage::StoredPlayer toStored(const PlayerSession& session, std::string_view world,
                                             std::int64_t firstSeenEpochSeconds, std::int64_t playTimeTicks,
                                             std::int64_t nowEpochSeconds);

void restore(PlayerSession& session, const storage::StoredPlayer& stored);

} // namespace basalt::server::runtime::player::playerPersistence
