#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

namespace cactusFeature {

bool place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// parity: `CactusBlock.canSurvive`, which is the placed feature's would-survive filter.
[[nodiscard]] bool canPlace(const PatchRegion& region, const DecorationPos& origin, ChunkHeight bounds);

[[nodiscard]] bool canPlace(Chunk& chunk, const DecorationPos& origin);

bool placeColumn(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin, ChunkHeight bounds);

} // namespace cactusFeature

namespace cactusPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name, ExternalStateReader externalBlockState = nullptr,
                     BlockWriteObserver observer = nullptr);

} // namespace cactusPatchDecoration

} // namespace basalt::server::world::terrain::decoration
