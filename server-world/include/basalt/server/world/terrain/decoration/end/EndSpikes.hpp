#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::world::terrain::decoration::end {

/// One obsidian pillar around the End's main island.
///
/// parity: `EndSpikeFeature.EndSpike`.
struct EndSpike {
    std::int32_t centerX = 0;
    std::int32_t centerZ = 0;
    std::int32_t radius = 0;
    std::int32_t height = 0;
    /// Whether an iron cage stands round its crystal.
    bool guarded = false;
};

namespace endSpikes {

inline constexpr std::int32_t kCount = 10;

/// parity: `EndSpikeFeature.getSpikesForLevel`: the ten pillars, their sizes shuffled from the seed.
[[nodiscard]] const std::vector<EndSpike>& forSeed(std::int64_t seed);

using SetBlock = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

/// parity: the blocks `EndSpikeFeature.placeSpike` writes before its crystal: the obsidian from the
/// bottom of the world, the air above sixty-five and, on a guarded pillar, the cage.
void placeBlocks(const EndSpike& spike, std::int32_t minY, const SetBlock& set);

} // namespace endSpikes

} // namespace basalt::server::world::terrain::decoration::end
