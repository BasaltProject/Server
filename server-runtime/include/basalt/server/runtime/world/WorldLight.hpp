#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

/// The light level at a position, from the same pass that fills the light packet.
///
/// The engine works a chunk at a time and keeps its answer on the chunk, so asking here costs a
/// lookup once that pass has run and a pass when it has not.
///
/// Whether the world has a sky is a parameter rather than something read off a world object: it is
/// the only thing these need besides the blocks, and taking it makes them testable without one.
namespace worldLight {

inline constexpr std::int32_t kMaxLevel = 15;

/// parity: `Level.getBrightness(LightLayer.SKY, pos)`, which is what a daylight detector reads.
[[nodiscard]] std::int32_t skyLight(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    bool hasSkylight = true);

/// parity: `Level.getBrightness(LightLayer.BLOCK, pos)`, what ice and snow ask before melting.
[[nodiscard]] std::int32_t blockLight(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LevelLightEngine.getRawBrightness(pos, skyDampen)`, the brighter of the two layers.
///
/// A dampen of zero matters: crops go by the sky light that reaches them, not by the time of day,
/// which is why a field keeps growing at night.
[[nodiscard]] std::int32_t rawBrightness(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                         bool hasSkylight = true, std::int32_t skyDampen = 0);

} // namespace worldLight

} // namespace basalt::server::runtime::world
