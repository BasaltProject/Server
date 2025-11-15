#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::world {

/// The water a sponge drinks the moment it is put down.
///
/// parity: `SpongeBlock.onPlace` and its `removeWaterBreadthFirstSearch`: six blocks out, sixty-five
/// of them at most, counted in the order a breadth-first walk reaches them. Water that a block is
/// merely logged with is drained out of it, and the kelp and seagrass standing in the way come out
/// with it. A sponge that drank anything at all turns wet.
namespace sponges {

/// Told about every block the sponge changed, its own included.
using OnChanged = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `WetSpongeBlock.onPlace` over the `water_evaporates` attribute, which is the Nether's: a
/// wet sponge put down there dries out on the spot.
///
/// `evaporates` is what that attribute answers for this dimension, which is the same thing as its
/// having a ceiling.
void dryIfEvaporating(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const OnChanged& onChanged,
                      bool evaporates);

/// Drinks whatever is around these coordinates and turns the sponge wet if it drank anything.
/// Reports every block that changed, the sponge included, so the caller can tell the clients.
void absorb(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const OnChanged& onChanged);

} // namespace sponges

} // namespace basalt::server::runtime::world
