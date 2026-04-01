#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>

#include <cstdint>

namespace basalt::server::runtime {
class FireworkSystem;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

class PlayerSession;

/// parity: `FireworkRocketItem.use`: a rocket stuck to whoever is gliding. Answers whether it was
/// used.
bool useRocketFor(const BreakContext& context, FireworkSystem& fireworks, PlayerSession& player,
                  world::LoadedWorld& world, std::int32_t slot);

/// parity: `FireworkRocketItem.useOn`: a rocket a hair off the face clicked, unless gliding.
bool useRocketOnFor(const BreakContext& context, FireworkSystem& fireworks, PlayerSession& player,
                    world::LoadedWorld& world, std::int32_t slot, const BlockHit& hit);

} // namespace basalt::server::runtime::player
