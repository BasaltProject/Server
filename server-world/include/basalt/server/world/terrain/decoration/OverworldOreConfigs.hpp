#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {

using BlockSet = std::set<std::string, std::less<>>;

struct Target {
    const BlockSet* replaceable;
    std::int32_t stateId;
    /// parity: 26.3's `height_match` on `#height_specific_ore_replaceables`.
    std::optional<std::pair<std::int32_t, std::int32_t>> heightSpecific;
};

/// parity: the 26.3 ore target rule: a height specific block by its y range, anything else by tag.
[[nodiscard]] bool targetMatches(const Target& target, std::string_view blockPath, std::int32_t y);

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
