#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;

bool wearAnvilAt(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                 float roll);

} // namespace basalt::server::runtime::player
