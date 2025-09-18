#pragma once

#include <basalt/server/world/terrain/decoration/OverworldOreConfigs.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// A disk of one block replacing another, the shape the four Overworld disks share.
struct DiskConfig {
    std::int32_t minRadius;
    std::int32_t maxRadius;
    std::int32_t halfHeight;
    std::set<std::int32_t> targets;
    std::function<std::int32_t(const PlacementRegion&, std::int32_t, std::int32_t, std::int32_t)> state;
};

[[nodiscard]] const DiskConfig& sandDisk();
[[nodiscard]] const DiskConfig& clayDisk();
[[nodiscard]] const DiskConfig& gravelDisk();
[[nodiscard]] const DiskConfig& grassDisk();

void placeDisk(PlacementRegion& region, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
               std::int32_t originZ, const DiskConfig& config);

/// parity: one complete configured `underwater_magma` body at the supplied placement origin.
bool placeUnderwaterMagmaBody(PlacementRegion& region, random::RandomSource& random, std::int32_t x, std::int32_t y,
                              std::int32_t z);

namespace overworldOreDecoration {

/// parity: the 28 ore, underwater magma and four disk placed features in the `UNDERGROUND_ORES` step.
///
/// The external readers see the surrounding region when there is one: ores write across chunk
/// borders and consume random testing those positions, so later features see what vanilla saw.
void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalHeightReader externalSurfaceHeight = nullptr, ExternalStateReader externalBlockState = nullptr);

/// parity: `ore_infested`, which runs in `UNDERGROUND_DECORATION` rather than with the other ores.
void decorateInfested(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      ExternalHeightReader externalSurfaceHeight = nullptr,
                      ExternalStateReader externalBlockState = nullptr);

/// The vanilla placed-feature names of the ores this step runs, by their global index. The index
/// seeds each feature's own random; the name is what `/place feature` says.
[[nodiscard]] const std::map<std::string, std::int32_t, std::less<>>& placedOreIds();

/// The vanilla placed-feature names of the disks this step runs.
[[nodiscard]] const std::set<std::string, std::less<>>& placedDiskIds();

/// One ore vein at one position: the feature without the modifiers that decide where it goes.
bool placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             std::string_view placedFeature);

/// One disk at one position, which is the feature without its scatter.
bool placeDiskAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::string_view placedFeature);

bool placeUnderwaterMagmaAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace overworldOreDecoration

} // namespace basalt::server::world::terrain::decoration
