#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {

using BlockSet = std::set<std::string, std::less<>>;

struct Target {
    const BlockSet* replaceable;
    std::int32_t stateId;
};

struct ConfiguredOre {
    std::int32_t size = 0;
    float discardChanceOnAirExposure = 0.0F;
    std::vector<Target> targets;
};

using Count = std::function<std::int32_t(random::RandomSource&)>;
using Height = std::function<std::int32_t(random::RandomSource&)>;

struct PlacedOre {
    std::int32_t globalIndex = 0;
    Count count;
    Height height;
    const BlockSet* biomes = nullptr;
    ConfiguredOre feature;
};

/// parity: the global feature index of `ore_infested` in `UNDERGROUND_DECORATION`.
inline constexpr std::int32_t kInfestedGlobalIndex = 6;

[[nodiscard]] const BlockSet& allBiomes();
[[nodiscard]] const BlockSet& defaultDiskBiomes();

/// parity: the `FeatureSorter` global indexes for the 55 Overworld biome feature lists in 26.2.
[[nodiscard]] const std::map<std::int32_t, PlacedOre>& oreFeaturesByIndex();
[[nodiscard]] const PlacedOre& infestedOre();

} // namespace basalt::server::world::terrain::decoration
