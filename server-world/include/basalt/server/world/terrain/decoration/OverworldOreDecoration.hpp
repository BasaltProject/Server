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
void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalHeightReader externalSurfaceHeight = nullptr, ExternalStateReader externalBlockState = nullptr);

/// parity: `ore_infested`.
void decorateInfested(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      ExternalHeightReader externalSurfaceHeight = nullptr,
                      ExternalStateReader externalBlockState = nullptr);

[[nodiscard]] const std::map<std::string, std::int32_t, std::less<>>& placedOreIds();

[[nodiscard]] const std::set<std::string, std::less<>>& placedDiskIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             std::string_view placedFeature);

bool placeDiskAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::string_view placedFeature);

bool placeUnderwaterMagmaAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace overworldOreDecoration

} // namespace basalt::server::world::terrain::decoration
