#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::world {

/// parity: `SpongeBlock.onPlace`.
namespace sponges {

using OnChanged = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `WetSpongeBlock.onPlace`.
void dryIfEvaporating(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const OnChanged& onChanged,
                      bool evaporates);

void absorb(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const OnChanged& onChanged);

} // namespace sponges

} // namespace basalt::server::runtime::world
