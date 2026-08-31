#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// Exact step-9 placement for the common and rare sweet-berry patches.
namespace berryPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// One named patch on its own, for `/place feature`.
void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name, ExternalStateReader externalBlockState = nullptr,
                     BlockWriteObserver observer = nullptr);

/// The patch at one origin, which the step calls after choosing a spot.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace berryPatchDecoration

/// Exact step-9 placement for `patch_pumpkin`.
namespace pumpkinPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace pumpkinPatchDecoration

} // namespace basalt::server::world::terrain::decoration
