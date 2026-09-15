#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;

/// Wears an anvil after a paid use; `roll` decides the twelve in a hundred chance of a step down.
/// Answers true when the block is gone. parity: `AnvilMenu.onTake` over `AnvilBlock.damage`.
bool wearAnvilAt(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                 float roll);

} // namespace basalt::server::runtime::player
