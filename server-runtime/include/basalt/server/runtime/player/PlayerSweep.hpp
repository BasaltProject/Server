#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

class PlayerSession;

/// One of the things a sweep caught beside what was aimed at.
struct SweepHit {
    std::int32_t entityId = 0;
    bool died = false;
};

/// A sword's sweep on everything around what was hit: damage, vibration, blame and panic. Telling
/// the clients is the caller's. parity: `Player.attack`'s sweep.
[[nodiscard]] std::vector<SweepHit> sweepAround(PlayerSession& session, world::LoadedWorld& world,
                                                const entity::ServerEntity& target, float strength);

} // namespace basalt::server::runtime::player
