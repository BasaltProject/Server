#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::player {

/// What telling a world that somebody opened a box needs beyond the world itself.
struct OpenersContext {
    /// Who is watching, and how a block change reaches them. The same one a break uses.
    BreakContext blocks;
    /// parity: `ChestBlockEntity.playSound` rolls a tenth of a tone before it plays.
    std::function<float()> nextFloat;
    std::function<std::int64_t()> nextSeed;
};

/// Tells everyone in that world that the count of people looking into a box changed.
///
/// parity: `ContainerOpenersCounter`: a lid on the first one in and off the last one out, a barrel
/// that opens its own block state instead, the sound on the same two edges and the vibration a
/// sculk sensor hears. Which of those happen is what `openers` says, not who asked.
void announceOpeners(const OpenersContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                     std::int32_t z, std::string_view block, std::int32_t openers);

} // namespace basalt::server::runtime::player
