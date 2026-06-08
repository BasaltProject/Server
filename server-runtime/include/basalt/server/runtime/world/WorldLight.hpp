#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

/// The engine keeps its answer on the chunk, so asking costs a lookup once its pass has run.
namespace worldLight {

inline constexpr std::int32_t kMaxLevel = 15;

/// parity: `Level.getBrightness(LightLayer.SKY, pos)`.
[[nodiscard]] std::int32_t skyLight(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    bool hasSkylight = true);

/// parity: `Level.getBrightness(LightLayer.BLOCK, pos)`.
[[nodiscard]] std::int32_t blockLight(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Fills the cache of the middle chunk of a window `WorldState::lightWindowFor` returned, off the tick thread.
void precompute(const std::array<server::world::Chunk*, 9>& window, bool hasSkylight);

/// Brings a loaded chunk's light up to date; false when not loaded. Safe from any thread.
bool refresh(WorldState& world, std::int32_t chunkX, std::int32_t chunkZ, bool hasSkylight);

/// parity: `LevelLightEngine.getRawBrightness(pos, skyDampen)`.
[[nodiscard]] std::int32_t rawBrightness(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                         bool hasSkylight = true, std::int32_t skyDampen = 0);

} // namespace worldLight

} // namespace basalt::server::runtime::world
