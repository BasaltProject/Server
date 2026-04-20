#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::world::terrain::decoration::end {

/// parity: `EndSpikeFeature.EndSpike`.
struct EndSpike {
    std::int32_t centerX = 0;
    std::int32_t centerZ = 0;
    std::int32_t radius = 0;
    std::int32_t height = 0;
    bool guarded = false;
};

namespace endSpikes {

inline constexpr std::int32_t kCount = 10;

/// parity: `EndSpikeFeature.getSpikesForLevel`.
[[nodiscard]] const std::vector<EndSpike>& forSeed(std::int64_t seed);

using SetBlock = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

/// parity: the blocks `EndSpikeFeature.placeSpike` writes before its crystal.
void placeBlocks(const EndSpike& spike, std::int32_t minY, const SetBlock& set);

} // namespace endSpikes

} // namespace basalt::server::world::terrain::decoration::end
